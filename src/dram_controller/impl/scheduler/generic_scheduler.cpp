#include <vector>

#include "base/base.h"
#include "dram_controller/controller.h"
#include "dram_controller/scheduler.h"

namespace Ramulator {

class FRFCFS : public IScheduler, public Implementation {
  RAMULATOR_REGISTER_IMPLEMENTATION(IScheduler, FRFCFS, "FRFCFS", "FRFCFS DRAM Scheduler.")

  private:
    IDRAM* m_dram;
    bool m_selected_ready = false;

  public:
    void init() override { };

    void setup(IFrontEnd* frontend, IMemorySystem* memory_system) override {
      m_dram = cast_parent<IDRAMController>()->m_dram;
    };

    ReqBuffer::iterator compare(ReqBuffer::iterator req1, ReqBuffer::iterator req2) override {
      bool ready1 = m_dram->check_ready(req1->command, req1->addr_vec);
      bool ready2 = m_dram->check_ready(req2->command, req2->addr_vec);

      if (ready1 ^ ready2) {
        if (ready1) {
          return req1;
        } else {
          return req2;
        }
      }

      // Fallback to FCFS
      if (req1->arrive <= req2->arrive) {
        return req1;
      } else {
        return req2;
      }
    }

    bool selected_request_is_ready() const override { return m_selected_ready; }

    ReqBuffer::iterator get_best_request(ReqBuffer& buffer) override {
      m_selected_ready = false;
      if (buffer.size() == 0) {
        return buffer.end();
      }

      // Update prerequisite commands for all requests.
      for (auto& req : buffer) {
        req.command = m_dram->get_preq_command(req.final_command, req.addr_vec);
      }

      // Optimization: compute each request's readiness only once
      // during this scheduling decision.
      auto candidate = buffer.begin();
      bool candidate_ready =
          m_dram->check_ready(candidate->command, candidate->addr_vec);

      for (auto next = std::next(buffer.begin(), 1);
           next != buffer.end();
           next++) {

        bool next_ready =
            m_dram->check_ready(next->command, next->addr_vec);

        if (candidate_ready ^ next_ready) {
          if (next_ready) {
            candidate = next;
            candidate_ready = next_ready;
          }
        } else {
          // Preserve the original FCFS tie-breaking rule.
          if (next->arrive < candidate->arrive) {
            candidate = next;
            candidate_ready = next_ready;
          }
        }
      }

      m_selected_ready = candidate_ready;
      return candidate;
    }
};

}  // namespace Ramulator
