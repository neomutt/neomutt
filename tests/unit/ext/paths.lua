local module = {}

module.include_paths = {}

module.test_libnvim_path = os.getenv('BUILD') .. '/libmutt.so'
-- module.test_helpers_path = os.getenv('BUILD') .. '/test_helpers.so'
module.source_path = os.getenv('SOURCE')
module.build_path = os.getenv('BUILD')
table.insert(module.include_paths, os.getenv('SOURCE'))
table.insert(module.include_paths, os.getenv('BUILD'))
table.insert(module.include_paths, os.getenv('SOURCE') .. "/tests/unit/ext/")

return module
