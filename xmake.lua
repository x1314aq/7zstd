add_rules("mode.debug", "mode.release")

target("7zstd")
    set_kind("binary")
    add_files("src/*.cpp", "thirdparty/fmt/*.cc", "thirdparty/lzma/*.c")
    add_includedirs("thirdparty/fmt", "thirdparty/lzma")
    set_languages("c11", "c++14")
    set_warnings("all")
    if is_plat("windows") then
        add_links("libzstd_static")
        add_includedirs("C:/Users/sapphire/software/usr/include")
        if is_mode("debug") then
            add_linkdirs("C:/Users/sapphire/software/usr/lib/debug")
        else
            add_linkdirs("C:/Users/sapphire/software/usr/lib/release")
        end
    else
        add_syslinks("pthread")
        add_links("zstd")
        add_includedirs("/home/huawei/.local/include")
        add_linkdirs("/home/huawei/.local/lib")
    end

--
-- If you want to known more usage about xmake, please see https://xmake.io
--
-- ## FAQ
--
-- You can enter the project directory firstly before building project.
--
--   $ cd projectdir
--
-- 1. How to build project?
--
--   $ xmake
--
-- 2. How to configure project?
--
--   $ xmake f -p [macosx|linux|iphoneos ..] -a [x86_64|i386|arm64 ..] -m [debug|release]
--
-- 3. Where is the build output directory?
--
--   The default output directory is `./build` and you can configure the output directory.
--
--   $ xmake f -o outputdir
--   $ xmake
--
-- 4. How to run and debug target after building project?
--
--   $ xmake run [targetname]
--   $ xmake run -d [targetname]
--
-- 5. How to install target to the system directory or other output directory?
--
--   $ xmake install
--   $ xmake install -o installdir
--
-- 6. Add some frequently-used compilation flags in xmake.lua
--
-- @code
--    -- add debug and release modes
--    add_rules("mode.debug", "mode.release")
--
--    -- add macro defination
--    add_defines("NDEBUG", "_GNU_SOURCE=1")
--
--    -- set warning all as error
--    set_warnings("all", "error")
--
--    -- set language: c99, c++11
--    set_languages("c99", "c++11")
--
--    -- set optimization: none, faster, fastest, smallest
--    set_optimize("fastest")
--
--    -- add include search directories
--    add_includedirs("/usr/include", "/usr/local/include")
--
--    -- add link libraries and search directories
--    add_links("tbox")
--    add_linkdirs("/usr/local/lib", "/usr/lib")
--
--    -- add system link libraries
--    add_syslinks("z", "pthread")
--
--    -- add compilation and link flags
--    add_cxflags("-stdnolib", "-fno-strict-aliasing")
--    add_ldflags("-L/usr/local/lib", "-lpthread", {force = true})
--
-- @endcode
--

