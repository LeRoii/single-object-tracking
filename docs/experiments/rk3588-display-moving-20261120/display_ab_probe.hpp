// Single-process diagnostic only. Shadow outputs never enter tracking/control.
#pragma once
#include "adaptive_display_box.hpp"
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <memory>
namespace display_ab_probe {
using namespace nanotrack;
inline std::unique_ptr<AdaptiveDisplayBox> off, on;
inline std::ofstream log;
inline void box(const BBox& b) { log << ',' << b.x << ',' << b.y << ',' << b.width << ',' << b.height; }
inline void initialize(const ControllerConfig& config, const BBox& b,
 const TargetClassState& target, const ImageView& image, std::uint64_t frame,
 std::int64_t timestamp, const BBox& actual) {
 const char* path=std::getenv("DISPLAY_AB_CSV"); if(!path) return;
 ControllerConfig cfg=config; cfg.display_stability_enabled=false;
 off=std::make_unique<AdaptiveDisplayBox>(cfg); cfg.display_stability_enabled=true;
 on=std::make_unique<AdaptiveDisplayBox>(cfg);
 off->initialize(b,target,{image.width,image.height},&image);
 on->initialize(b,target,{image.width,image.height},&image);
 log.open(path); if(!log) throw std::runtime_error("cannot open DISPLAY_AB_CSV");
 log << std::fixed << std::setprecision(6)
 << "frame_id,timestamp_ms,tracking_phase,accepted,reacquired,off_x,off_y,off_width,off_height,on_x,on_y,on_width,on_height,actual_x,actual_y,actual_width,actual_height,off_source,on_source\n";
 log << frame << ',' << timestamp << ",TRACKING,1,0"; box(off->current());box(on->current());box(actual);log << ",INITIALIZED,INITIALIZED\n";
}
inline void update(const ControllerOutput& output,const TargetClassState& target,
 const std::optional<DetectorBatch>& batch,const ImageView& image,const BBox& actual) {
 if(!off) return;
 if(batch){off->ingest(*batch,output,target);on->ingest(*batch,output,target);}
 AdaptiveDisplayDiagnostics a,b;
 const BBox x=off->update(output,target,a,&image),y=on->update(output,target,b,&image);
 log << output.frame_id << ',' << output.timestamp_ms << ',' << to_string(output.tracking_phase)
 << ',' << output.accepted << ',' << output.reacquired;
 box(x);box(y);box(actual);log << ',' << to_string(a.source) << ',' << to_string(b.source) << '\n';
}
inline void observe(float ms){if(off){off->observe_runtime(ms);on->observe_runtime(ms);}}
}
