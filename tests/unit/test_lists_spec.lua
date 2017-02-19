local helpers = require('tests.unit.ext.helpers')
local Paths = require('tests.unit.ext.paths')

local ffi        = helpers.ffi
local mutt       = helpers.lib

local eq         = helpers.eq
local NULL       = helpers.NULL
local address_of = helpers.address_of

-- Type abstraction

MuttList = function(o)
  _ml = o or mutt.mutt_new_list()
  ffi.gc(_ml, mutt.mutt_free_list)
  return {
    add = function(val)
      if type(val) == 'string' then
        _ml = mutt.mutt_add_list(_ml, val)
      end
    end,
    front = function(self)
      return ffi.string(mutt.mutt_front_list(_ml[0]))
    end,
    find = function(val)
      local elt = mutt.mutt_find_list(_ml, val)
      if elt == NULL then
        return false
      end
      return MuttList(elt)
    end,
    contains = function(val)
      local elt = mutt.mutt_find_list(_ml, val)
      if elt == NULL then
        return false
      end
      elt = MuttList(elt)
      return elt.front() == val
    end,
    push = function(val)
      if type(val) == 'string' then
        local ml_ptr = ffi.cast("struct List **", address_of(_ml))
        mutt.mutt_push_list(ml_ptr, val)
        _ml = ml_ptr[0]
      end
    end,
    pop = function()
      local elt = MuttList(_ml).front()
      local ml_ptr = ffi.cast("struct List **", address_of(_ml))
      local rv = mutt.mutt_pop_list(ml_ptr)
      _ml = ml_ptr[0]
      if elt ~= NULL and rv ~= 0 then
        return ffi.string(elt)
      end
      return ""
    end
  }
end

-- Test specs

describe('Testing lists API', function()
  describe('for which list interface', function()
    it('handles insertion', function()
      local l = MuttList()
      l.add('fubar1')
      l.add('fubar2')
      l.add('fubar3')
    end)
    it('handles find', function()
      local l = MuttList()
      l.add('fubar1')
      l.add('fubar2')
      l.add('fubar3')

      eq(true, l.contains('fubar1'))
      eq(true, l.contains('fubar2'))
      eq(true, l.contains('fubar3'))
      eq(false, l.contains('fubar4'))
    end)
  end)
  describe('for which stack interface', function()
    it('handles push', function()
      local l = MuttList()
      l.push('fubar1')
      eq('fubar1', l.front())
      l.push('fubar2')
      eq('fubar2', l.front())
      l.push('fubar3')
      eq('fubar3', l.front())
    end)
    it('handles find', function()
      local l = MuttList()
      l.push('fubar1')
      l.push('fubar2')
      l.push('fubar3')
      eq(true, l.contains('fubar3'))
      eq(true, l.contains('fubar2'))
      eq(true, l.contains('fubar1'))
      eq(false, l.contains('fubar4'))
    end)
    it('handles find', function()
      local l = MuttList()
      l.push('fubar1')
      l.push('fubar2')
      l.push('fubar3')
      eq('fubar3', l.pop())
      eq('fubar2', l.pop())
      eq('fubar1', l.pop())
      eq('', l.pop())
    end)
  end)
end)
