project('perch', 'cpp',
  version: '0.4.0',
  meson_version: '>= 1.3.0',
  default_options: ['cpp_std=c++20', 'warning_level=2', 'buildtype=release'])

qt6 = import('qt6')

qt_dep = dependency('qt6',
  modules: ['Core', 'Gui', 'Widgets', 'Network', 'WebEngineWidgets'])

# Dependências do KDE Frameworks 6 para o diálogo "Sobre"
kf6_coreaddons_dep = dependency('KF6CoreAddons', required: true)
kf6_i18n_dep = dependency('KF6I18n', required: true)
kf6_xmlgui_dep = dependency('KF6XmlGui', required: true)

moc_files = qt6.compile_moc(
  headers: ['src/popup_window.h', 'src/settings_dialog.h'],
  dependencies: qt_dep)

resources = qt6.compile_resources(sources: 'data/resources.qrc')

# Translations: needs Qt's lrelease; without it the app still builds, in English only.
lrelease = find_program('lrelease-qt6', 'lrelease6', 'lrelease',
  qt_dep.get_variable(pkgconfig: 'bindir', default_value: '') / 'lrelease',
  required: false)
extra_sources = []
if lrelease.found()
  subdir('translations')
  extra_sources += i18n_resources
else
  warning('lrelease not found: building WITHOUT translations (install the Qt Linguist tools)')
endif

executable('perch',
  [
    'src/main.cpp',
    'src/popup_window.cpp',
    'src/settings_dialog.cpp',
    'src/about.cpp',
    moc_files,
    resources,
    extra_sources
  ],
  dependencies: [qt_dep, kf6_coreaddons_dep, kf6_i18n_dep,kf6_xmlgui_dep],
  install: true)

app_id = 'io.FlaipyTheHost.Perch'
install_data('data' / app_id + '.desktop',
  install_dir: get_option('datadir') / 'applications')
install_data('data' / app_id + '.svg',
  install_dir: get_option('datadir') / 'icons/hicolor/scalable/apps')
