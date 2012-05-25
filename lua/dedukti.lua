-- Dedukti LUA basic runtime.

-- Terms can be of 6 kinds, either a lambda, a product, an
-- application, type, or a box.
--
-- In a term object, two fields must be present, 'tk',
-- indicating the kind of the term, and a field with
-- the name of the kind.

-- Code can be of 6 kinds, either a lambda, a product, a
-- rule, a constant, type, or kind.

tlam, tlet, tpi, tapp, ttype, tbox = -- Possible tk
  'tlam', 'tlet', 'tpi', 'tapp', 'ttype', 'tbox';

clam, cpi, ccon, ctype, ckind =      -- Possible ck
  'clam', 'cpi', 'ccon', 'ctype', 'ckind';

function var(n)
  return { ck = ccon, ccon = "var" .. n, args = {} };
end

function box(ty, t)
  return { tk = tbox, tbox = { ty, t } };
end

local function push(c, v)
  local a = {};
  for i=1,#c.args do
    a[i] = c.args[i];
  end
  table.insert(a, v);
  if c.ck == clam then
    return { ck = clam, clam = c.clam, arity = c.arity, args = a };
  else
    return { ck = ccon, ccon = c.ccon, args = a };
  end
end

function ap(a, b)
  assert(a.ck and b.ck);
  if a.ck == clam then      -- Apply a rewrite rule/lambda.
    local c = push(a, b);
    if #c.args == c.arity then
      return c.clam(unpack(c.args));
    else
      return c;
    end
  elseif a.ck == ccon then  -- Apply a constant.
    return push(a, b);
  end
end

function conv(n, a, b)
  assert(a.ck and b.ck);
  local v = var(n);
  if a.ck == cpi and b.ck == cpi then
    return conv(n, a.cpi[1], b.cpi[1])
       and conv(n+1, a.cpi[2](v), b.cpi[2](v));
  elseif a.ck == clam and b.ck == clam then
    return conv(n+1, ap(a, v), ap(b, v));
  elseif a.ck == ccon and b.ck == ccon
     and a.ccon == b.ccon and #a.args == #b.args then
    if #a.args == 0 then
      return true;
    end
    for i=1,#a.args-1 do
      if not conv(n, a.args[i], b.args[i]) then
        return false;
      end
    end
    return conv(n, a.args[#a.args], b.args[#b.args]);
  elseif a.ck == ctype and b.ck == ctype then
    return true;
  elseif a.ck == ckind and b.ck == ckind then
    return true;
  else
    print("Terms are not convertible:");
    print("    " .. strc(a));
    print("    " .. strc(b));
    return false;
  end
end

--[[ Typechecking functions. ]]

function synth(n, t)
  assert(t.tk);
  if t.tk == tbox then
    return t.tbox[1];
  elseif t.tk == ttype then
    return { ck = ckind };
  elseif t.tk == tlet then
    return synth(n, t.tlet[3](t.tlet[1], t.tlet[2]));
  elseif t.tk == tapp then
    local c = synth(n, t.tapp[1]);
    assert(c.ck == cpi and check(n, t.tapp[2], c.cpi[1]));
    return c.cpi[2](t.tapp[3]);
  else
    error("Type synthesis failed.");
  end
end

function check(n, t, c)
  assert(t.tk and c.ck);
  if t.tk == tlam and c.ck == cpi then
    local v = var(n);
    return check(n+1, t.tlam[2](box(c.cpi[1], v), v), c.cpi[2](v));
  elseif t.tk == tpi then
    if not check(n, t.tpi[1], { ck = ctype }) then
      error("Type checking failed: Invalid product.");
    end
    local v = var(n);
    return check(n+1, t.tpi[3](box(t.tpi[2], v), v), c);
  elseif t.tk == tlet then
    return check(n, t.tlet[3](t.tlet[1], t.tlet[2]), c);
  else
    return conv(n, synth(n, t), c);
  end
end

function chktype(t)
  if not check(0, t, { ck = ctype }) then
    error("Type checking failed: Sort error.");
  end
end

function chkkind(t)
  if not check(0, t, { ck = ckind }) then
    error("Type checking failed: Sort error.");
  end
end

function chk(t, c)
  if not check(0, t, c) then
    error("Type checking failed: Terms are not convertible.");
  end
end

--[[ Utility functions. ]]

local indent = 0;
local function shiftp(m)
  print(string.rep("  ", indent) .. m);
end

function chkbeg(x)
  shiftp("Checking " .. x .. ".");
  indent = indent + 1;
end

function chkmsg(x)
  shiftp(x);
end

function chkend(x)
  indent = indent - 1;
  shiftp("Done checking \027[32m" .. x .. "\027[m.");
end

--[[ Debugging functions. ]]

function strc(c)
  local function f(n, c)
    if c.ck == clam then
      return "(\\" .. n .. ". " .. f(n+1, ap(c, var(n))) ..")";
    elseif c.ck == cpi then
      return "(Pi " .. n .. ":" .. f(n, c.cpi[1])
          .. ". " .. f(n+1, c.cpi[2](var(n))) .. ")";
    elseif c.ck == ccon then
      local s = "(" .. c.ccon;
      for i=1,#c.args do
        s = s .. " " .. f(n, c.args[i]);
      end
      return s .. ")";
    elseif c.ck == ctype then
      return "Type";
    elseif c.ck == ckind then
      return "Kind";
    else
      return "!! not code !!";
    end
  end
  return f(0, c);
end

--[[ Miscelaneous. ]]

-- Allows to load .dko files using require.
package.path = "./?.dko;" .. package.path;

-- vi: expandtab: sw=2
