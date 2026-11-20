"""Apply a diagnostic-only shadow display probe to a disposable source copy."""
from pathlib import Path
import sys, shutil
root=Path(sys.argv[1]);p=root/'src/long_term_runtime.cpp';t=p.read_text()
assert 'display_ab_probe' not in t
replacements=[('#include "target_class.hpp"','#include "target_class.hpp"\n#include "display_ab_probe.hpp"'),
('  result.display_bbox = adaptive_display_.current();','  result.display_bbox = adaptive_display_.current();\n  display_ab_probe::initialize(controller_.config(), initial_bbox, result.target_class, image, frame_id, timestamp_ms, result.display_bbox);'),
('  result.output.bbox = result.output.display_bbox;','  display_ab_probe::update(result.output, result.target_class, display_batch, image, result.output.display_bbox);\n  result.output.bbox = result.output.display_bbox;'),
('  adaptive_display_.observe_runtime(result.runtime_ms);','  adaptive_display_.observe_runtime(result.runtime_ms);\n  display_ab_probe::observe(result.runtime_ms);')]
for old,new in replacements:
 assert t.count(old)==1,(old,t.count(old));t=t.replace(old,new)
p.write_text(t);shutil.copy(Path(__file__).with_name('display_ab_probe.hpp'),root/'src/display_ab_probe.hpp')
