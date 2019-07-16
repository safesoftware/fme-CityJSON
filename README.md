![CityJSON Format Plugin for FME](https://is3-ssl.mzstatic.com/image/thumb/Purple118/v4/31/9c/c7/319cc748-5ac6-2d91-8b1a-afdc7e3e164e/AppIcon-1x_U007emarketing-0-0-GLES2_U002c0-512MB-sRGB-0-0-0-85-220-0-0-0-6.png/246x0w.jpg)

![CityJSON](https://www.cityjson.org/en/0.8/_static/cityjson_logo.svg)

# CityJSON Format Plugin for FME

This repository contains the source code of the CityJSON Format Plugin for FME.

## Description
The CityJSON Format Plugin for FME ...

[CityJSON](https://www.cityjson.org) is a format for encoding a subset of the CityGML data model (version 2.0.0) using JavaScript Object Notation (JSON). A CityJSON file represents both the geometry and the semantics of the city features of a given area, eg buildings, roads, rivers, the vegetation, and the city furniture.

[FME](https://www.safe.com) is the all-in-one tool for data integration and productivity.

## Requirements
FME 2018.0 or later installed on either
* Linux
* Mac
* Windows

## Limitation
We are working toward full CityJSON support.  We have tested ...

## How to build
Requires this [JSON parser](https://github.com/nlohmann/json)

For CMake place the `json.hpp` file to `./includes/nlohmann/json.hpp` so it can find it.

### Linux
Do the usual CMake build setup in the source dir.
```
mkdir build && cd build
cmake -DFME_DEV_HOME=/path/to/fme/installation/dir ..
make && make install
```
The option `-DFME_DEV_HOME` is required for both linking the FME libraries and installing the plugin. See the *Installation Instructions* below for the details, and the `CMakeLists.txt`.
### Mac
Same (probably) as Linux
### Windows
Set an environment variable FME_DEV_HOME to be the path to the directory where FME is installed.  For example, C:\Program Files\FME
In Visual Studio (2017 or later), open `fmecityjson.sln`.
Select either the `x64` or `x86` configuration, depending on if you want to build 64-bit or 32-bit versions, respectively.  Your choice of 64-bit or 32-bit must match the version of FME you are running.
The solution contains a Debug and Release
configuration. When a project is built in the Debug
configuration, debug information can be retrieved when
running project.
Build the plug-ins by selecting Build > Build
Solution from the menu.
To verify the build was successful, select View > Error List
from the menu. In the Error List pane, check that there
are 0 errors.

## Installation Instructions
There are several steps necessary to extend FME to include this CityJSON Format support.

* The Plugin:
**Build the CityJSON plugin, using the instructions above.  This will produce a file `fmecityjson.so` file on Linux, `fmecityjson.so` file on Mac, or a `fmecityjson.dll` file on Windows.  Copy this file into the `plugins` subdirectory where FME is installed.
* The Format Information File :
** Copy the file `cityjson.db` into the `formatsinfo` subdirectory where FME is installed.
(This file will supply the Reader and
Writer Gallery with information about the format, including
whether to display the format in the gallery, if it supports
coordinate systems, and the default file extensions.
For more information on the parameters in format files, see the file `formats.txt` in the directory where FME is installed.)
* The Metafile:
** Copy the file `cityjson.fmf` into the `metafile` subdirectory where FME is installed.
(The metafile contains directives that inform FME of the
details about a format such as default parameters, settings,
schemas and so on.)
* Restart FME

## Licenses
* [JSON parser](https://github.com/nlohmann/json/blob/master/LICENSE)
