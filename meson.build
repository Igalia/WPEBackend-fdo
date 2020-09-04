project('WPEBackend-fdo', ['c', 'cpp'],
	meson_version: '>=0.49',
	default_options: [
		'b_ndebug=if-release',
		'c_std=c99',
		'cpp_eh=none',
		'cpp_std=c++11',
	],
	license: 'BSD-2-Clause',
	version: '1.7.90',
)

# This refers to the API level provided. This is modified only with major,
# breaking changes, which need modifications in programs using the library
# before they can be compiled again.
api_version = '1.0'

# Before making a release, the soversion array should be modified.
# The string is of the form [C, R, A].
# - If interfaces have been changed or added, but binary compatibility has
#   been preserved, change to [C+1, 0, A+1].
# - If binary compatibility has been broken (eg removed or changed interfaces)
#   change to [C+1, 0, 0]
# - If the interface is the same as the previous version, use [C, R+1, A].
soversion = [7, 2, 6]

# Split the *project* version into its components.
version_info = meson.project_version().split('.')
version_info = {
	'PROJECT_VERSION_MAJOR': version_info[0],
	'PROJECT_VERSION_MINOR': version_info[1],
	'PROJECT_VERSION_PATCH': version_info[2],
}

# Mangle [C, R, A] into an actual usable *soversion*.
soversion_major = soversion[0] - soversion[2]  # Current-Age
soversion_minor = soversion[2]  # Age
soversion_micro = soversion[1]  # Revision
soversion = '@0@.@1@.@2@'.format(soversion_major, soversion_minor, soversion_micro)

sources = [
	'src/exported-buffer-shm.cpp',
	'src/exported-image-egl.cpp',
	'src/fdo.cpp',
	'src/initialize-egl.cpp',
	'src/initialize-eglstream.cpp',
	'src/initialize-shm.cpp',
	'src/ipc.cpp',
	'src/renderer-backend-egl.cpp',
	'src/renderer-host.cpp',
	'src/version.c',
	'src/view-backend-exportable-fdo.cpp',
	'src/view-backend-exportable-fdo-egl.cpp',
	'src/view-backend-exportable-fdo-eglstream.cpp',
	'src/view-backend-exportable-private.cpp',
	'src/ws.cpp',
	'src/ws-client.cpp',
	'src/ws-egl.cpp',
	'src/ws-eglstream.cpp',
	'src/ws-shm.cpp',
	'src/extensions/audio.cpp',
	'src/extensions/audio-receiver.cpp',
	'src/extensions/video-plane-display-dmabuf.cpp',
	'src/extensions/video-plane-display-dmabuf-receiver.cpp',
	'src/linux-dmabuf/linux-dmabuf.cpp',
	'src/linux-dmabuf/linux-dmabuf-protocol.c',
]

api_headers = [
	'include/wpe/exported-image-egl.h',
	'include/wpe/exported-buffer-shm.h',
	'include/wpe/initialize-egl.h',
	'include/wpe/view-backend-exportable.h',
	'include/wpe/view-backend-exportable-egl.h',
	'include/wpe/fdo-egl.h',
	'include/wpe/fdo.h',

	# Generated API headers.
	configure_file(
		input: 'include/wpe/version.h.cmake',
		output: 'version.h',
		configuration: version_info,
	),
]

extensions_api_headers = [
	'include/wpe/extensions/audio.h',
	'include/wpe/extensions/video-plane-display-dmabuf.h',
]

unstable_api_headers = [
	'include/wpe/unstable/fdo-eglstream.h',
	'include/wpe/unstable/fdo-shm.h',
	'include/wpe/unstable/initialize-shm.h',
	'include/wpe/unstable/initialize-eglstream.h',
	'include/wpe/unstable/view-backend-exportable-eglstream.h',
]

add_project_arguments(
	'-DWPE_FDO_COMPILATION=1',
	'-DG_LOG_DOMAIN="WPE-FDO"',
	'-D_LARGEFILE64_SOURCE=1',
	language: ['c', 'cpp']
)

wayland_scanner_dep = dependency('wayland-scanner', required: false, native: true)
if wayland_scanner_dep.found()
	wayland_scanner = find_program(
		wayland_scanner_dep.get_pkgconfig_variable('wayland_scanner'),
		native: true
	)
else
	wayland_scanner = find_program('wayland-scanner', native: true)
endif

r = run_command(wayland_scanner, '--version')
assert(r.returncode() == 0,
	'''Cannot execute wayland-scanner:
	''' + r.stderr())
wayland_scanner_version = r.stderr().split()
if not (wayland_scanner_version.length() == 2)
	wayland_scanner_version = r.stdout().split()
endif
wayland_scanner_code = 'code'
if wayland_scanner_version.length() == 2
	wayland_scanner_version = wayland_scanner_version[1]
	if wayland_scanner_version.version_compare('>=1.15')
		wayland_scanner_code = 'private-code'
	endif
	message('wayland-scanner version: ' + wayland_scanner_version)
else
	wayland_scanner_version = ''
	message('wayland-scanner version: unknown')
endif
message('Using "' + wayland_scanner_code + '" as parameter to wayland-scanner')

# Wayland extension: WPE bridge.
wpe_bridge_client_proto_header = custom_target(
	'wpe-bridge-client-proto-header',
	input: 'src/bridge/wpe-bridge.xml',
	output: '@BASENAME@-client-protocol.h',
	command: [wayland_scanner, 'client-header', '@INPUT@', '@OUTPUT@'],
)
wpe_bridge_server_proto_header = custom_target(
	'wpe-bridge-server-proto-header',
	input: 'src/bridge/wpe-bridge.xml',
	output: '@BASENAME@-server-protocol.h',
	command: [wayland_scanner, 'server-header', '@INPUT@', '@OUTPUT@'],
)
wpe_bridge_proto_source = custom_target(
	'wpe-bridge-proto-source',
	input: 'src/bridge/wpe-bridge.xml',
	output: '@BASENAME@-protocol.c',
	command: [wayland_scanner, wayland_scanner_code, '@INPUT@', '@OUTPUT@'],
)

# Wayland extension: DMA-BUF video plane display.
wpe_video_plane_display_dmabuf_client_proto_header = custom_target(
	'video-plane-display-dmabuf-client-proto-header',
	input: 'src/extensions/wpe-video-plane-display-dmabuf.xml',
	output: '@BASENAME@-client-protocol.h',
	command: [wayland_scanner, 'client-header', '@INPUT@', '@OUTPUT@'],
)
wpe_video_plane_display_dmabuf_server_proto_header = custom_target(
	'video-plane-display-dmabuf-server-proto-header',
	input: 'src/extensions/wpe-video-plane-display-dmabuf.xml',
	output: '@BASENAME@-server-protocol.h',
	command: [wayland_scanner, 'server-header', '@INPUT@', '@OUTPUT@'],
)
wpe_video_plane_display_dmabuf_proto_source = custom_target(
	'video-plane-display-dmabuf-proto-source',
	input: 'src/extensions/wpe-video-plane-display-dmabuf.xml',
	output: '@BASENAME@-protocol.c',
	command: [wayland_scanner, wayland_scanner_code, '@INPUT@', '@OUTPUT@'],
)

# Wayland extension: Audio rendering protocol.
wpe_audio_client_proto_header = custom_target(
	'audio-client-proto-header',
	input: 'src/extensions/wpe-audio.xml',
	output: '@BASENAME@-client-protocol.h',
	command: [wayland_scanner, 'client-header', '@INPUT@', '@OUTPUT@'],
)
wpe_audio_server_proto_header = custom_target(
	'audio-server-proto-header',
	input: 'src/extensions/wpe-audio.xml',
	output: '@BASENAME@-server-protocol.h',
	command: [wayland_scanner, 'server-header', '@INPUT@', '@OUTPUT@'],
)
wpe_audio_proto_source = custom_target(
	'audio-proto-source',
	input: 'src/extensions/wpe-audio.xml',
	output: '@BASENAME@-protocol.c',
	command: [wayland_scanner, wayland_scanner_code, '@INPUT@', '@OUTPUT@'],
)

# Wayland extension: EGLStream controller
wayland_eglstream_controller_server_proto_header = custom_target(
	'wayland-eglstream-controller-server-proto-header',
	input: 'src/wayland-eglstream/wayland-eglstream-controller.xml',
	output: '@BASENAME@-server-protocol.h',
	command: [wayland_scanner, 'server-header', '@INPUT@', '@OUTPUT@'],
)

wayland_eglstream_controller_proto_source = custom_target(
	'wayland-eglstream-controller-proto-source',
	input: 'src/wayland-eglstream/wayland-eglstream-controller.xml',
	output: '@BASENAME@-protocol.c',
	command: [wayland_scanner, wayland_scanner_code, '@INPUT@', '@OUTPUT@'],
)

cxx = meson.get_compiler('cpp')

# Switch to the 'cpp_eh=none' default option when updating to Meson 0.51 or newer, see
# https://mesonbuild.com/Builtin-options.html
if meson.version().version_compare('<0.51')
	add_project_arguments(
		cxx.get_supported_arguments(['-fno-exceptions']),
		language: 'cpp'
	)
endif

# Switch to the 'cpp_rtti=false' default option when updating to Meson 0.53 or newer, see
# https://mesonbuild.com/FAQ.html#how-do-i-disable-exceptions-and-rtti-in-my-c-project
if meson.version().version_compare('<0.53')
	add_project_arguments(
		cxx.get_supported_arguments(['-fno-rtti']),
		language: 'cpp'
	)
endif

add_project_arguments(
	cxx.get_supported_arguments(['-fvisibility=hidden']),
	language: 'cpp'
)
add_project_arguments(
	meson.get_compiler('c').get_supported_arguments(['-fvisibility=hidden']),
	language: 'c'
)

deps = [
	dependency('epoxy'),
	dependency('gio-2.0'),
	dependency('gobject-2.0'),
	dependency('wayland-client'),
	dependency('wayland-server'),
	dependency('wayland-egl'),
	dependency('wpe-1.0', version: '>=1.5.90'),
]

xkbcommon_dep = dependency('xkbcommon', required: false)
if xkbcommon_dep.found()
	deps += [xkbcommon_dep]
elif not cxx.has_header('xkbcommon/xkbcommon-keysyms.h')
	error('The <xkbcommon/xkbcommon-keysyms.h> header is missing')
endif

generated_sources = [
	wpe_bridge_proto_source,
	wpe_bridge_client_proto_header,
	wpe_bridge_server_proto_header,
	wpe_audio_proto_source,
	wpe_audio_client_proto_header,
	wpe_audio_server_proto_header,
	wpe_video_plane_display_dmabuf_proto_source,
	wpe_video_plane_display_dmabuf_client_proto_header,
	wpe_video_plane_display_dmabuf_server_proto_header,
	wayland_eglstream_controller_proto_source,
	wayland_eglstream_controller_server_proto_header,
]

lib = shared_library(meson.project_name() + '-' + api_version,
	sources, generated_sources,
	install: true,
	dependencies: deps,
	version: soversion,
	soversion: soversion_major,
	include_directories: include_directories('include'),
)

wpebackend_fdo_dep = declare_dependency(link_with : lib,
  include_directories : include_directories('include'),
  dependencies : deps,
  sources : generated_sources,
)

install_headers(api_headers,
	subdir: join_paths('wpe-fdo-' + api_version, 'wpe'),
)
install_headers(extensions_api_headers,
	subdir: join_paths('wpe-fdo-' + api_version, 'wpe', 'extensions'),
)
install_headers(unstable_api_headers,
	subdir: join_paths('wpe-fdo-' + api_version, 'wpe', 'unstable'),
)

import('pkgconfig').generate(
	description: 'The WPEBackend-fdo library',
	name: 'wpebackend-fdo-' + api_version,
	subdirs: 'wpe-fdo-' + api_version,
	libraries: lib,
	version: meson.project_version(),
)

if get_option('build_docs')
	hotdoc = import('hotdoc')
	assert(hotdoc.has_extensions('c-extension'),
		'The HotDoc C extension is required.'
	)
	libwpe_doc = hotdoc.generate_doc(meson.project_name(),
		project_version: api_version,
		index: 'docs/index.markdown',
		sitemap: 'docs/sitemap.txt',
		console: true,
		install: true,
		build_by_default: true,
		c_smart_index: true,
		c_sources: api_headers,
		c_include_directories: ['include', meson.current_build_dir()],
		# The space here is relevant, see
		# https://github.com/mesonbuild/meson/issues/5800#issuecomment-552198354
		extra_c_flags: [' -DWPE_FDO_COMPILATION=1'],
	)
endif
