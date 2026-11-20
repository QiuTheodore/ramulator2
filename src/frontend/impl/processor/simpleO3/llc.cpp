#include <iostream>
#include "frontend/impl/processor/simpleO3/llc.h"

namespace Ramulator {

SimpleO3LLC::SimpleO3LLC(int latency, int size_bytes, int linesize_bytes, int associativity, int num_mshrs):
m_latency(latency), m_size_bytes(size_bytes), m_linesize_bytes(linesize_bytes), m_associativity(associativity), m_num_mshrs(num_mshrs) {
  m_logger = Logging::create_logger("SimpleO3LLC");

  m_set_size = m_size_bytes / (m_linesize_bytes * m_associativity);
  m_index_mask = m_set_size - 1;
  m_index_offset = calc_log2(m_linesize_bytes);
  m_tag_offset = calc_log2(m_set_size) + m_index_offset;

  // Cache set indices are fixed by the configured LLC geometry.
  m_cache_sets.resize(m_set_size);

  DEBUG_LOG(DSIMPLEO3LLC, m_logger, "Index mask: {0:x}", m_index_mask);
  DEBUG_LOG(DSIMPLEO3LLC, m_logger, "Index offset: {}",  m_index_offset);
  DEBUG_LOG(DSIMPLEO3LLC, m_logger, "Tag offset: {}",    m_tag_offset);
};

void SimpleO3LLC::tick() {
  m_clk++;

  // Send miss requests to the memory system when LLC latency is met
  // TODO: Optimization by assuming in-order issue?
  auto it = m_miss_list.begin(); 
  while (it != m_miss_list.end()) {
    if (m_clk >= it->first) {
      if (!m_memory_system->send(it->second)) {
        it++;
      }
      else {
        it = m_miss_list.erase(it);
      }
    } else {
      // All entries are appended with m_clk + fixed m_latency.
      // Due clocks are nondecreasing; later entries cannot be due.
      break;
    }
  }

  // call hit request callback when LLC latency is met
  it = m_hit_list.begin();
  while (it != m_hit_list.end()) {
    if (m_clk >= it->first) {
      std::vector<Request> _req_v{it->second};
      m_receive_requests[it->second.addr] = _req_v;

      it->second.callback(it->second);
      it = m_hit_list.erase(it);
    } 
    else {
      // The hit queue is ordered by completion cycle as well.
      break;
    }
  }
};

bool SimpleO3LLC::send(Request req) {
  CacheSet_t& set = get_set(req.addr);

  if (req.type_id == Request::Type::Read) {
    s_llc_read_access++;
  } else if (req.type_id == Request::Type::Write) {
    s_llc_write_access++;
  }

  if (auto line_it = check_set_hit(set, req.addr); line_it != set.end()) {
    // Hit in the set
    DEBUG_LOG(DSIMPLEO3LLC, m_logger, 
    "[Clk={}] Request Source: {}, Type: {}, Addr: {}, Index: {}, Tag: {}. Hit, will finish at Clk={}", 
    m_clk, req.source_id, req.type_id, req.addr, get_index(req.addr), get_tag(req.addr), m_clk, m_clk + m_latency
    );

    // Update LRU in O(1) without allocating and freeing a list node.
    // Keep the same updated fields as the previous push_back/erase path.
    line_it->addr = req.addr;
    line_it->tag = get_tag(req.addr);
    line_it->dirty = line_it->dirty || (req.type_id == Request::Type::Write);
    line_it->ready = true;
    set.splice(set.end(), set, line_it);

    // Add to the hit list to callback when finished
    m_hit_list.push_back(std::make_pair(m_clk + m_latency, req));
    return true;
  } else {
    // Miss in the set
    DEBUG_LOG(DSIMPLEO3LLC, m_logger, 
    "[Clk={}] Request Source: {}, Type: {}, Addr: {}, Index: {}, Tag: {}. Miss.", 
    m_clk, req.source_id, req.type_id, req.addr, get_index(req.addr), get_tag(req.addr), m_clk, m_clk + m_latency
    );

    if (req.type_id == Request::Type::Read) {
      s_llc_read_misses++;
    } else if (req.type_id == Request::Type::Write) {
      s_llc_write_misses++;
    }

    bool dirty = (req.type_id == Request::Type::Write);
    if (req.type_id == Request::Type::Write) {
      req.type_id = Request::Type::Read;
    }

    // MSHR lookup
    auto mshr_it = check_mshr_hit(req.addr);
    if (mshr_it != m_mshrs.end()) {
      DEBUG_LOG(DSIMPLEO3LLC, m_logger,  "MSHR Hit.", m_clk);
      // Add new req to MSHR_requests
      m_receive_requests[mshr_it->first].push_back(req);

      mshr_it->second->dirty = dirty || mshr_it->second->dirty;
      return true;
    }

    // MSHR miss
    // Check if there is available MSHR entry
    if (m_mshrs.size() == m_num_mshrs) {
      DEBUG_LOG(DSIMPLEO3LLC, m_logger,  "No MSHR entry available.", m_clk);
      s_llc_mshr_unavailable++;
      return false;
    }

    // Check if there is available cache line in the set
    bool line_available = false;
    if (set.size() < m_associativity) {
      line_available = true;
    } else {
      for (const auto& line : set) {
        if (line.ready) {
          line_available = true;
        }
      }
    }
    if (!line_available) {
      DEBUG_LOG(DSIMPLEO3LLC, m_logger,  "No cache line available in the set.", m_clk);
      return false;
    }

    // Allocate a new cache line
    auto newline_it = allocate_line(set, req.addr);
    if (newline_it == set.end()) {
      // Should this happen?
      throw std::runtime_error("Failed to allocate new line when there is available entry.");
      return false;
    }
    newline_it->dirty = dirty;
    
    // Add to MSHR entries
    m_mshrs.push_back(std::make_pair(req.addr, newline_it));
    // Add Request to MSHR_requests
    std::vector<Request> _req_v{req};
    m_receive_requests[req.addr] = _req_v;

    // Add to the miss request list
    m_miss_list.push_back(std::make_pair(m_clk + m_latency, req));

    return true;
  }
};

void SimpleO3LLC::receive(Request& req) {
  auto it = std::find_if(
    m_mshrs.begin(), m_mshrs.end(),
    [&req, this](MSHREntry_t mshr_entry) { return (align(mshr_entry.first) == align(req.addr)); }
  );

  DEBUG_LOG(DSIMPLEO3LLC, m_logger, "[Clk={}] Request {} received.", m_clk, req.addr);

  if (it != m_mshrs.end()) {
    it->second->ready = true;
    m_mshrs.erase(it);
  }
};

SimpleO3LLC::CacheSet_t& SimpleO3LLC::get_set(Addr_t addr) {
  return m_cache_sets[get_index(addr)];
}

SimpleO3LLC::CacheSet_t::iterator SimpleO3LLC::allocate_line(CacheSet_t& set, Addr_t addr) {
  // Check if we need to evict any line
  if (need_eviction(set, addr)) {
    // Get a victim to evict
    auto victim = std::find_if(set.begin(), set.end(), [this](Line line) { return line.ready; });
    if (victim == set.end())
      return victim;  // doesn't exist a line that's already unlocked in each level
    evict_line(set, victim);
  }

  // Allocate new cache line and return an iterator to it
  set.push_back({addr, get_tag(addr)});
  return --set.end();
}

bool SimpleO3LLC::need_eviction(const CacheSet_t& set, Addr_t addr) {
  const Addr_t tag = get_tag(addr);
  if (std::find_if(set.begin(), set.end(),
            [tag](const Line& line) { return tag == line.tag; })
      != set.end()) {
    // Due to MSHR, the program can't reach here. Just for checking
    assert(false);
    return false;
  } 
  else {
    if (set.size() < m_associativity) {
      return false;
    } else {
      return true;
    }
  }
}

void SimpleO3LLC::evict_line(CacheSet_t& set, CacheSet_t::iterator victim_it) {
  DEBUG_LOG(DSIMPLEO3LLC, m_logger,  "Evicting {}.", victim_it->addr);
  s_llc_eviction++;

  // Generate writeback request if victim line is dirty
  if (victim_it->dirty) {
    Request writeback_req(victim_it->addr, Request::Type::Write);
    m_miss_list.push_back(std::make_pair(m_clk + m_latency, writeback_req));

    DEBUG_LOG(DSIMPLEO3LLC, m_logger,  "Writeback Request will be issued at Clk={}.", m_clk + m_latency);
  }

  set.erase(victim_it);
}


SimpleO3LLC::CacheSet_t::iterator SimpleO3LLC::check_set_hit(CacheSet_t& set, Addr_t addr) {
  const Addr_t tag = get_tag(addr);
  auto line_it = std::find_if(set.begin(), set.end(),
      [tag](const Line& line) { return line.tag == tag; });
  // Do not dereference end(): the original code had undefined behavior
  // on cache misses. A missing or in-flight line is not a cache hit.
  return line_it != set.end() && line_it->ready ? line_it : set.end();
}

SimpleO3LLC::MSHR_t::iterator SimpleO3LLC::check_mshr_hit(Addr_t addr) {
  auto mshr_it =
    std::find_if(
      m_mshrs.begin(), m_mshrs.end(),
      [addr, this](MSHREntry_t mshr_entry) { return (align(mshr_entry.first) == align(addr)); }
    );
  return mshr_it;
}

void SimpleO3LLC::serialize(std::string serialization_filename) {
  std::ofstream serialization_file;
  serialization_file.open(serialization_filename, std::ios::out);

  serialization_file << "index,addr,tag,dirty" << std::endl;
  for (size_t index = 0; index < m_cache_sets.size(); ++index) {
    for (const auto& line : m_cache_sets[index]) {
      serialization_file << index << "," << line.addr << "," << line.tag << "," << line.dirty << std::endl;
    }
  }
  serialization_file.close();
}

void SimpleO3LLC::deserialize(std::string serialization_filename) {
  std::ifstream serialization_file;
  serialization_file.open(serialization_filename, std::ios::out);

  std::string file_line;
  std::getline(serialization_file, file_line); // Skip the first line, which is the header
  while (std::getline(serialization_file, file_line)) {
    std::string index_str = file_line.substr(0, file_line.find(","));
    file_line = file_line.substr(file_line.find(",") + 1);
    std::string addr_str = file_line.substr(0, file_line.find(","));
    file_line = file_line.substr(file_line.find(",") + 1);
    std::string tag_str = file_line.substr(0, file_line.find(","));
    file_line = file_line.substr(file_line.find(",") + 1);
    std::string dirty_str = file_line.substr(0, file_line.find(","));
    
    int index = std::stoi(index_str);
    Addr_t addr = std::stoll(addr_str);
    Addr_t tag = std::stoll(tag_str);
    bool dirty = std::stoi(dirty_str);
    if (index < 0 || index >= m_set_size) {
      throw std::runtime_error("Invalid cache set index in LLC serialization");
    }
    m_cache_sets[index].push_back({addr, tag, dirty, 1});
  }
  serialization_file.close();
}

void SimpleO3LLC::dump_llc() {
  /**
   * @brief dumps the LLC cache to the console
   * 
   */
  std::cout << "Dumping LLC" << std::endl;
  std::cout << "index,addr,tag,dirty,ready" << std::endl;
  for (size_t index = 0; index < m_cache_sets.size(); ++index) {
    for (const auto& line : m_cache_sets[index]) {
      std::cout << index << "," << line.addr << "," << line.tag << "," << line.dirty << "," << line.ready << std::endl;
    }
  }
}

}        // namespace Ramulator