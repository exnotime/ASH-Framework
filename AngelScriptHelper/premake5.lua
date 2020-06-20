solution "Ash"
    configurations { "Debug", "Release" }
        flags{ "NoPCH" }
        includedirs { "include" }
        libdirs {"libs"}
        platforms{"x64" }

        local location_path = "solution"
        if _ACTION then
	        defines { "_CRT_SECURE_NO_WARNINGS", "NOMINMAX"  }
            location_path = location_path .. "/" .. _ACTION
            location ( location_path )
            location_path = location_path .. "/projects"
        end
    disablewarnings { "4251" }
    systemversion ("10.0") --use latest
    configuration { "Debug" }
        defines { "DEBUG" }
        symbols "On"
        targetdir ( "bin/" .. "/debug" )

    configuration { "Release" }
        defines { "NDEBUG", "RELEASE" }
        optimize "on"
        floatingpoint "fast"
        targetdir ( "bin/" .. "/release" )

	project "Ash"
        targetname "Ash"
		debugdir ""
        defines { "AS_USE_NAMESPACE"}
		location ( location_path )
		language "C++"
		kind "ConsoleApp"
		files { "src/**"}
        includedirs { "include", "src/**", "ash_lib_src" }
        staticruntime "On"
        links { "AshLib" }
        configuration { "Debug" }
                links { "angelscript64d" }
        configuration { "Release" }
                links { "angelscript64" }

    project "AshLib"
        targetname "AshLib"
        defines { "AS_USE_NAMESPACE"}
		debugdir ""
		location ( location_path )
		language "C++"
		kind "StaticLib"
		files { "ash_lib_src/AngelScriptExporter.h", "ash_lib_src/AngelScriptExporter.cpp"}
        includedirs { "include" }
        staticruntime "On"
