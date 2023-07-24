project(
    'ta-remake-updater', 'cpp',
    default_options: 'cpp_std=c++20'
)

cxx = meson.get_compiler('cpp')
curl_dep = dependency('libcurl', static: true)
archive_dep = dependency('libarchive', static: true)
deps = [curl_dep, archive_dep]

sources = files('main.cpp')
output_dir = meson.build_root() + '/output'

executable(
    'updater', sources,
    dependencies: deps,
    cpp_args: ['-DCURL_STATICLIB'],
    link_args: ['-static'],
    install: true,
    install_dir: output_dir
)
