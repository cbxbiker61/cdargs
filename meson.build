project('cdargs', 'cpp',
        version : '2.1',
        license : 'GPLv2',
        default_options: [
                'cpp_std=c++11',
                'prefix=/usr',
                'sysconfdir=/etc',
                'localstatedir=/var',
        ],
       )

mandir = join_paths (get_option ('prefix'), get_option ('mandir'))
conf = configuration_data()
# Surround the version in quotes to make it a C string
conf.set_quoted('VERSION', meson.project_version())
configure_file(output : 'config.h', configuration : conf)

src = 'src/cdargs.cpp'
ncurses_dep = dependency ('ncurses')

executable('cdargs', src, dependencies : [ncurses_dep], install : true)
install_man('doc/cdargs.1')
