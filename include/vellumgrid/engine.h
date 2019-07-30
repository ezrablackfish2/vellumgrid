#pragma once

#include "vellumgrid/job_format.h"
#include "vellumgrid/subsystems.h"

#include <cstddef>
#include <cstdint>

namespace vellumgrid {

class DocumentEngine {
 public:
  DocumentEngine();
  void apply(const Command& command, uint32_t batch_nonce);
  uint64_t render_all(uint32_t salt);
  uint64_t replay_pending(uint32_t salt);
  uint64_t pressure_cycle(uint32_t salt);
  uint64_t digest() const noexcept;

 private:
  void begin_document(uint32_t id, uint32_t nonce);
  bool accepts(const Command& command) const noexcept;

  uint32_t document_id_{};
  uint32_t last_epoch_{};
  uint64_t digest_{};
  bool active_{};

  FontBook fonts_;
  StyleCatalog styles_;
  TextStore texts_;
  PageBook pages_;
  AnnotationMap annotations_;
  ImageStore images_;
  PathStore paths_;
  RenderPlanner planner_;
  RenderCache render_cache_;
  TransactionManager transactions_;
  ProfileStore profiles_;
  RenderScheduler scheduler_;
  ResourcePool resource_pool_;
  Clipboard clipboard_;
  DependencyMap dependencies_;
  MeshCache mesh_cache_;
};

uint64_t run_job(const JobBatch& batch, uint8_t mode);

}  // namespace vellumgrid
