project('phosphor-pid-autotune', 'cpp',
  default_options : ['cpp_std=c++20', 'warning_level=2'])

# Dependencies
boost_dep = dependency('boost', required : true)
sdbusplus_dep = dependency('sdbusplus', method : 'pkg-config', required : true)
phosphor_logging_dep = dependency('phosphor-logging', required : false)
json_dep = dependency('nlohmann_json', method : 'pkg-config', required : true)

inc = include_directories('.')

srcs = [
  'main.cpp',
  'buildjson/buildjson.cpp',
  'core/numeric.cpp',
  'core/steady_state.cpp',
  'core/sensorinfo.cpp',
  'core/sysfs_io.cpp',
  'core/time_utils.cpp',
  'core/units.cpp',
  'dbus/dbusconfiguration.cpp',
  'experiment/base_duty.cpp',
  'experiment/step_trigger.cpp',
  'process_models/fopdt.cpp',
  'PID_tuning_methods/imc.cpp',
]

executable('phosphor-pid-autotune',
  srcs,
  include_directories : inc,
  dependencies : [boost_dep, sdbusplus_dep, json_dep, phosphor_logging_dep],
  install : true,
  install_dir : get_option('bindir')
)

install_data('configs/autotune.json',
  install_dir : join_paths(get_option('datadir'), 'phosphor-pid-autotune', 'configs'))
