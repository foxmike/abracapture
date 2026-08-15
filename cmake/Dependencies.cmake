find_package(Qt6 REQUIRED COMPONENTS Core Quick QuickControls2)
find_package(PkgConfig REQUIRED)

pkg_check_modules(PIPEWIRE REQUIRED IMPORTED_TARGET libpipewire-0.3)
pkg_check_modules(SNDFILE REQUIRED IMPORTED_TARGET sndfile)
pkg_check_modules(FLAC REQUIRED IMPORTED_TARGET flac)
