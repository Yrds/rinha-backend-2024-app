project('rinha-2024-q1-project', 'cpp',
  version : '0.1',
  default_options : ['warning_level=3',
                     'cpp_std=c++23'])

deps = []
deps += subproject('cpp-httplib').get_variable('cpp_httplib_dep')
deps += subproject('sqlite3').get_variable('sqlite3_dep')
deps += subproject('nlohmann_json').get_variable('nlohmann_json_dep')

src = files(
  'src/rinha_2024_q1_project.cpp',
  'src/database.cpp'
  )

exe = executable('rinha-2024-q1-project', src,
  install : true, dependencies: deps)

test('basic', exe)
