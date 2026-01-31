project(
    'phosphor-pid-autotune',
    'cpp',
    version: '0.1',
    meson_version: '>=0.57.0',
    default_options: [
        'warning_level=3',
        'werror=false',
        # Updated to c++23 for std::move_only_function support in sdbusplus
        'cpp_std=c++23', 
        'buildtype=debugoptimized',
    ],
)

sdbusplus = dependency('sdbusplus')
nlohmann_json = dependency('nlohmann_json')
systemd = dependency('systemd')
boost = dependency('boost')

deps = [
    sdbusplus,
    nlohmann_json,
    systemd,
    boost,
]

inc = include_directories('.')

srcs = [
    'core/dbus_io.cpp',
    'core/utils.cpp',
    'buildjson/config.cpp',
    'experiment/step_trigger.cpp',
    'process_models/fopdt.cpp',

    'main.cpp',
]

executable(
    'phosphor-pid-autotune',
    srcs,
    dependencies: deps,
    include_directories: inc,
    install: true,
)

systemd_system_unit_dir = systemd.get_variable('systemdsystemunitdir')


conf_data = configuration_data()
conf_data.set('BINDIR', get_option('prefix') / get_option('bindir'))
conf_data.set('DATADIR', get_option('prefix') / get_option('datadir'))

configure_file(
    input: 'phosphor-pid-autotune.service.in',
    output: 'phosphor-pid-autotune.service',
    configuration: conf_data,
    install: true,
    install_dir: systemd_system_unit_dir,
)

install_data(
    'configs/autotune.json',
    install_dir: get_option('datadir') / 'phosphor-pid-autotune' / 'configs',
)