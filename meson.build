project('btop', 'cpp',
  version : '1.0.20',
  default_options : ['cpp_std=c++20', 'warning_level=3'])

src_files = files([
  'src/btop_config.cpp',
  'src/btop_draw.cpp',
  'src/btop_input.cpp',
  'src/btop_menu.cpp',
  'src/btop_theme.cpp',
  'src/btop_tools.cpp',
  'src/linux/btop_collect.cpp',
  'src/btop.cpp'
])

add_global_arguments('-DMESON_BUILD', language : ['c', 'cpp'])

config_data = configuration_data()
config_data.set_quoted('CONFIG_BTOP_VERSION', meson.project_version())

configure_file(
  output : 'config.h',
  configuration: config_data,
)

src_inc = include_directories(['src'])
thirdparty_inc = include_directories(['include'])

threads_dep = dependency('threads')

install_data('README.md', install_dir : get_option('datadir') / 'btop')
install_subdir('themes', install_dir : get_option('datadir') / 'btop')

exe = executable(
  'btop',
  sources: src_files,
  dependencies: [threads_dep],
  include_directories: [src_inc, thirdparty_inc],
  install : true,
)
