--[[ Modify this section to add more tests. ]]

do
    local unit_tests = {
        { name = "bug_sort", result = nil },
        { name = "bug_abs", result = nil },
        { name = "exemple", result = true },
        { name = "peano", result = true, deps = { "coc", "logic" } },
        { name = "f", result = true },
        -- { name = "compute", result = true },
        { name = "dotpat", result = true},
        { name = "qualpat", result = true },
        { name = "scope", result = true },
    }

    tests = { unit = unit_tests }
end

--[[ Test execution. ]]

local verbose, path = false -- Test program options.

-- Default path to the directory just above.
do
    local f = io.popen("cd ..; pwd")
    path = f:lines()()
    f:close()
end

-- Console tools.
local function green(s) return "\027[32m" .. s .. "\027[m" end
local function   red(s) return "\027[31m" .. s .. "\027[m" end

function dkcheck(cat, f, deps)
    local function clamp(ret)
        if _VERSION ~= "Lua 5.1" then return ret end
        if ret ~= 0 then return nil else return true end
    end

    local fpath, dpath = f .. ".dk", ""
    if deps then
        for _, d in ipairs(deps) do
            dpath = dpath .. " " .. d .. ".dk"
        end
    end
    local luacmd = string.format("LUA_PATH=%s/lua/?.lua lua -l dedukti -", path)
    local cmd    = string.format("%s/dkparse %s %s 2>/dev/null | %s", path, dpath, fpath, luacmd)
    cmd = string.format("cd %s/test/%s; %s", path, cat, cmd)
    if verbose then
        print("Running command: " .. cmd)
        return clamp(os.execute(cmd))
    else
        return clamp(os.execute(cmd .. " 2>/dev/null >/dev/null"))
    end
end

function runtest(cat, i, t)
    local o = io.output()
    o:write(string.format("[TEST %02d] Running test %s... ", i, t.name))
    o:flush()
    if dkcheck(cat, t.name, t.deps) == t.result then
        o:write(green("ok\n"))
    else
        o:write(red("failed\n"))
        failed = failed + 1
    end
end

-- Parse arguments.
for o = 1, #arg do
    if arg[o] == "-p" and arg[o+1] then
        path = arg[o+1]
    elseif arg[o] == "-v" then
        verbose = true
    elseif arg[o] == "-h" then
        print("usage: do.lua [-p PATH] [-v] [-h]")
        print("\t-p\tSet the root path of the project.")
        print("\t-v\tShow typechecking as it is done.")
        print("\t-h\tDisplay this help message.")
        return
    end
end

-- Run tests.
failed = 0
for cat, tlist in pairs(tests) do
    print("\t-- Running test from category " .. cat .. " --");
    for i, t in ipairs(tlist) do
        runtest(cat, i, t)
    end
    print("");
end

if failed ~= 0 then
    print(string.format("\n%s test(s) failed!", red(failed)))
else
    print("\nAll tests succeeded!");
end
