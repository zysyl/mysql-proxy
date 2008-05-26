@echo "Run this from a shell started with the Visual Studio Build environment set!"
@echo "You can set DEPS_PATH (for the dependencies package) and MYSQL_DIR for a MySQL server installation"
@IF DEFINED DEPS_PATH (GOTO MYSQL_CONF)
@SET DEPS_PATH=%CD%\..\..\mysql-lb-deps\win32

:MYSQL_CONF
@IF DEFINED MYSQL_DIR (GOTO GENERAL_CONF)
@SET MYSQL_DIR="C:\Program Files\MySQL\MySQL Server 5.0"

:GENERAL_CONF
@SET GLIB_DIR=%DEPS_PATH%
@SET PATH=%DEPS_PATH%\bin;%PATH%
@SET NSISDIR=%DEPS_PATH%\bin

@echo Using MySQL server from %MYSQL_DIR%
@echo Using dependencies from %DEPS_PATH%

@echo Checking for NSIS...
@reg query HKLM\Software\NSIS /v VersionMajor
@IF %ERRORLEVEL% NEQ 0 (GOTO NONSIS)
@GOTO NSISOK

:NONSIS
@echo using internal NSIS installation
@SET CLEANUP_NSIS=1

@reg add HKLM\Software\NSIS /ve /t REG_SZ /d %NSISDIR% /f
@reg add HKLM\Software\NSIS /v VersionMajor /t REG_DWORD /d 00000002 /f
@reg add HKLM\Software\NSIS /v VersionMinor /t REG_DWORD /d 00000025 /f
@reg add HKLM\Software\NSIS /v VersionRevision /t REG_DWORD /d 0 /f
@reg add HKLM\Software\NSIS /v VersionBuild /t REG_DWORD /d 0 /f

@GOTO ENDNSIS

:NSISOK
@echo found existing NSIS installation
@SET CLEANUP_NSIS=0
@GOTO ENDNSIS

:ENDNSIS

@rem MSVC 8 2005 doesn't seem to have devenv.com
@SET VS_CMD="%VS80COMNTOOLS%\..\IDE\VCExpress.exe"

@echo Copying dependencies to deps folder
@copy %DEPS_PATH%\packages\* deps\

@rem clear the cache if neccesary to let cmake recheck everything
@rem del CMakeCache.txt
 
@cmake -DMYSQL_LIBRARY_DIRS:PATH=%MYSQL_DIR%\lib\debug -DMYSQL_INCLUDE_DIRS:PATH=%MYSQL_DIR%\include -DGLIB_LIBRARY_DIRS:PATH=%GLIB_DIR%\lib -DGLIB_INCLUDE_DIRS:PATH=%GLIB_DIR%\include\glib-2.0;%GLIB_DIR%\lib\glib-2.0\include -DGMODULE_INCLUDE_DIRS:PATH=%GLIB_DIR%\include\glib-2.0;%GLIB_DIR%\lib\glib-2.0\include -DGTHREAD_INCLUDE_DIRS:PATH=%GLIB_DIR%\include\glib-2.0;%GLIB_DIR%\lib\glib-2.0\include .

%VS_CMD% mysql-proxy.sln /Clean
%VS_CMD% mysql-proxy.sln /Build Release
%VS_CMD% mysql-proxy.sln /Build Release /project RUN_TESTS
%VS_CMD% mysql-proxy.sln /Build Release /project PACKAGE
%VS_CMD% mysql-proxy.sln /Build Release /project INSTALL

@rem if you use VS8 to build then VS80COMNTOOLS should be set
@rem "%VS80COMNTOOLS%\..\IDE\devenv.com" mysql-proxy.sln /Clean
@rem "%VS80COMNTOOLS%\..\IDE\devenv.com" mysql-proxy.sln /Build
@rem "%VS80COMNTOOLS%\..\IDE\devenv.com" mysql-proxy.sln /Build Debug /project RUN_TESTS
@rem "%VS80COMNTOOLS%\..\IDE\devenv.com" mysql-proxy.sln /Build Debug /project PACKAGE
@rem "%VS80COMNTOOLS%\..\IDE\devenv.com" mysql-proxy.sln /Build Debug /project INSTALL


@IF %CLEANUP_NSIS% EQU 1 (GOTO REMOVEKEYS)
@echo leaving existing keys untouched
@GOTO END

:REMOVEKEYS
@echo removing temporary NSIS registry entries
@reg delete HKLM\Software\NSIS /va /f

:END
