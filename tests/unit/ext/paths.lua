local module = {}

module.include_paths = {}

module.test_libnvim_path = os.getenv('BUILD_DIR') .. '/libmutt.so'
-- module.test_helpers_path = os.getenv('BUILD_DIR') .. '/test_helpers.so'
module.source_path = os.getenv('SOURCE_DIR')
module.build_path = os.getenv('BUILD_DIR')
table.insert(module.include_paths, os.getenv('SOURCE_DIR'))
table.insert(module.include_paths, os.getenv('BUILD_DIR'))
table.insert(module.include_paths, os.getenv('SOURCE_DIR') .. "/tests/unit/ext/")

return module
