if mutt and mutt.message then
  print = mutt.message
end

found, runner = pcall(require, 'busted.runner')
if not found then
  print("Please install busted, e.g. with luarocks:")
  print(" %% luarocks install busted")
  os.exit(1)
end

runner()

local eq = function(exp, act)
  return assert.are.same(exp, act)
end
local neq = function(exp, act)
  return assert.are_not.same(exp, act)
end
local ok = function(res)
  return assert.is_true(res)
end

local test_config_type = function(setting, a, b)
  eq(a, mutt.get(setting))
  mutt.set(setting, b)
  eq(b, mutt.get(setting))
end


describe('lua API', function()
  describe('test get/set', function()
    it('works with DT_STRING', function()
      test_config_type("visual", "vim", "fubar")
    end)

    it('works with DT_NUMBER and positive ints', function()
      test_config_type("connect_timeout", 69, 42)
      test_config_type("connect_timeout", 42, 69)
    end)

    it('works with DT_NUMBER and negative ints', function()
      test_config_type("connect_timeout", 69, -42)
      test_config_type("connect_timeout", -42, 69)
    end)

    it('works with DT_NUMBER and does not accept positive int overflow', function()
      assert.has_error(function() test_config_type("connect_timeout", 69, 33000) end)
    end)

    it('works with DT_NUMBER and does not accept negative int overflow', function()
      assert.has_error(function() test_config_type("connect_timeout", 69, -33000) end)
    end)

    it('works with DT_BOOL', function()
      test_config_type("arrow_cursor", true, false)
    end)

    it('works with DT_QUAD', function()
      test_config_type("abort_noattach", mutt.QUAD_NO, mutt.QUAD_ASKNO)
    end)

    it('works with DT_PATH', function()
      test_config_type("alias_file", "contrib/lua/test_lua-api_runner.neomuttrc", "/dev/null")
    end)

    it('works with DT_MAGIC', function()
      test_config_type("mbox_type", "mbox", "Maildir")
    end)

    it('works with DT_SORT', function()
      test_config_type("sort", "from", "date")
    end)

    it('works with DT_REGEX', function()
      test_config_type("mask", "!^\\\\.[^.]", ".*")
    end)

    it('works with DT_MBTABLE', function()
      test_config_type("to_chars", "+TCFL", "+T")
    end)

    it('works with DT_ADDRESS', function()
      test_config_type("from", "fubar@example.org", "barfu@example.com")
    end)

    it('works with custom my_ variable DT_STRING', function()
      test_config_type("my_fubar", "Ford Prefect", "Zaphod Beeblebrox")
    end)

    it('detects a non-existent my_ variable DT_STRING', function()
      assert.has_error(function() mutt.get("my_doesnotexists") end)
    end)

    it('detects a non-existent other variable', function()
      assert.has_error(function() mutt.get("doesnotexists") end)
    end)
  end)
end)
