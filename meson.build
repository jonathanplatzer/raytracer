project(
    'raytracer',
    'c',
    version: 'indev',
    license: 'proprietary',
    meson_version: '>= 0.48.0',
    default_options: ['c_std=c99']
)

sources = files(
    'src/main.c'
)

cc = meson.get_compiler('c')
m = cc.find_library('m', required: false)

executable(
    'raytracer',
    sources,
    dependencies: [m]
)

# vim: shiftwidth=4 tabstop=4
