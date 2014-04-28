{
	'variables': {
		'node_shared_openssl%': 'true'
	},
	'targets': [
	{
		'target_name': 'native_stuff',
		'sources': [
			'native/dtls.cpp',
			'native/srtp.cpp',
			'native/helper.cpp',
		'native/module.cpp'
			],
		'conditions': [
			['node_shared_openssl=="false"', {
				'include_dirs': [
					'<(node_root_dir)/deps/openssl/openssl/include'
					],
				"conditions" : [
					["target_arch=='ia32'", {
						"include_dirs": [ "<(node_root_dir)/deps/openssl/config/piii" ]
					}],
				["target_arch=='x64'", {
					"include_dirs": [ "<(node_root_dir)/deps/openssl/config/k8" ]
				}],
				["target_arch=='arm'", {
					"include_dirs": [ "<(node_root_dir)/deps/openssl/config/arm" ]
				}]
				]
			}]
		],
			'cflags': [
				'-std=c++11',
			'-Wall',
			'-g',
			],
			'ldflags': [
			'-lsrtp',
			],
			'defines': [
				'DO_DEBUG'
			],
	}
	]
}
