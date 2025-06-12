-- premake5.lua
workspace "WalnutApp"
   architecture "x64"
   configurations { "Debug", "Release", "Dist" }
   startproject "WalnutApp"

outputdir = "%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}"

includedirs { "RS-232" }
files { "RS-232/rs232.c", "RS-232/**.h" }
include "Walnut/WalnutExternal.lua"

include "WalnutApp"