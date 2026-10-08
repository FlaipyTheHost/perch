    find . -exec touch {} +
    meson setup build --prefix=$HOME/.local
    meson compile -C build
    meson install -C build
    perch &
