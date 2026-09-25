#!/usr/bin/env bash
set -euo pipefail
python3 tools/embed_runtime_web.py --check
node --check web/runtime.js
node --check web/pose_comparison.js
node tools/test_pose_comparison.js
node --check site/serial-monitor.js
node tools/test_serial_monitor.js
node tools/test_runtime_web.js
python3 tools/test_rwlog_v46_converter.py
for name in runtime_control export_protocol previous_peak_math diagnostic_journal; do
  g++ -std=c++17 -O2 -Wall -Wextra -Werror -Itools/host_v46o -Isrc tools/test_${name}.cpp src/control_work_profile.cpp -o /tmp/test_${name}
  /tmp/test_${name}
done
g++ -std=c++17 -O2 -Wall -Wextra -Werror -Itools/host_v46o -Isrc tools/test_camera_task_patch.cpp src/camera_task_priority_patch.cpp -o /tmp/test_camera_task_patch
/tmp/test_camera_task_patch
g++ -std=c++17 -O2 -Wall -Wextra -Werror -Itools/host_v46o tools/test_foot_tracking.cpp src/white_marker_tracker.cpp src/foot_angle_estimator.cpp -o /tmp/test_foot
/tmp/test_foot
g++ -std=c++11 -O2 tools/test_v46n_acquisition.cpp -o /tmp/test_acq
/tmp/test_acq
g++ -std=c++17 -O2 -Wall -Wextra -Werror -Itools/host_v46o tools/test_v46o_startup.cpp -o /tmp/test_startup
/tmp/test_startup | tail -n 1
g++ -std=c++17 -O2 tools/test_mekf_host.cpp src/mekf6.cpp -o /tmp/test_mekf
/tmp/test_mekf
g++ -std=c++17 -O2 -Wall -Wextra -Werror tools/test_mekf_sparse_equivalence.cpp src/mekf6.cpp tools/fixtures/mekf6_dense_reference_0478.cpp -o /tmp/test_mekf_sparse
/tmp/test_mekf_sparse
g++ -std=c++17 -Os -Wall -Wextra -Werror tools/test_mekf_sparse_equivalence.cpp src/mekf6.cpp tools/fixtures/mekf6_dense_reference_0478.cpp -o /tmp/test_mekf_speed_policy
/tmp/test_mekf_speed_policy
python3 tools/verify_madgwick_dependency.py
g++ -std=c++17 -Os -Wall -Wextra -Werror tools/test_madgwick_pitch.cpp tools/fixtures/adafruit_ahrs_2_4_0/Adafruit_AHRS_Madgwick.cpp -o /tmp/test_madgwick_pitch
/tmp/test_madgwick_pitch
g++ -std=c++17 -Os -Wall -Wextra -Werror tools/test_log_quantization.cpp -o /tmp/test_log_quantization
/tmp/test_log_quantization
g++ -std=c++17 -O2 -Wall -Wextra -Werror -Itools/host_v46o tools/test_mekf_diagnostics.cpp src/mekf6.cpp -o /tmp/test_mekf_diagnostics
/tmp/test_mekf_diagnostics /tmp/mekf-diagnostics-fixture.json
g++ -std=c++17 -O2 -Wall -Wextra -Werror -Itools/host_v46o tools/test_control_work_profile.cpp src/control_work_profile.cpp -o /tmp/test_control_work
/tmp/test_control_work /tmp/control-work-fixture.json
g++ -std=c++17 -O2 -Wall -Wextra -Werror -Wno-format -ffunction-sections -fdata-sections -Itools/host_v46o tools/test_runtime_logger.cpp src/psram_logger.cpp src/foot_observer.cpp src/foot_angle_estimator.cpp src/white_marker_tracker.cpp src/immutable_export.cpp src/control_work_profile.cpp -Wl,--gc-sections -o /tmp/test_logger
/tmp/test_logger /tmp/runtime-fixture.rwlog
python3 tools/test_runtime_fixture.py
