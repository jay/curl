Ref: https://github.com/curl/curl/issues/12327

This document shows how to build libcurl with **CURLDEBUG** (libcurl heap
memory tracking) or **CRTDBG** (Windows CRT heap memory tracking). The latter
will track libcurl and the application.

The example commands use `%CL_CRTDBG%` for **CRTDBG**, but can be changed to
`%CL_CURLDEBUG%` for **CURLDEBUG**.

To force a memory leak in both libcurl and the application (curl.exe or
simple.exe) add `/DCURL_FORCEMEMLEAK` to the CL options. This is useful to see
what the output of a memory leak looks like. It should cause 7 bytes to leak
from libcurl and 5 bytes to leak from the application.

Open a Visual Studio command prompt and build curl:
~~~cmd
set "CURL_SRCPATH=x:\j\curl\curl"
set "CL_CRTDBG=cl /DCURL_FORCEMEMLEAK /FC /FIcrtdbg.h /D_CRTDBG_MAP_ALLOC /D_CRT_SECURE_NO_WARNINGS /D_CRT_NONSTDC_NO_DEPRECATE"
set "CL_CURLDEBUG=cl /DCURL_FORCEMEMLEAK /FC /DCURLDEBUG /DMEMDEBUG_LOG_SYNC"

cd /d "%CURL_SRCPATH%\winbuild"
if not exist ..\src\tool_hugehelp.c (..\buildconf.bat)
nmake /f Makefile.vc mode=dll VC=10 DEBUG=yes clean
nmake /f Makefile.vc mode=dll VC=10 DEBUG=yes CC="%CL_CRTDBG%"
~~~

The build output will show an object directory ending in **-obj-curl**. Remove
that suffix and switch to the bin output subdir. For example if the object
directory is `libcurl-vc10-x86-debug-dll-ipv6-sspi-schannel-obj-curl`:
~~~cmd
cd ..\builds\libcurl-vc10-x86-debug-dll-ipv6-sspi-schannel\bin
~~~

If the curl tool (curl.exe) is not the application then build the application:
~~~cmd
%CL_CRTDBG% /MDd /I..\include "%CURL_SRCPATH%\docs\examples\simple.c" /link /LIBPATH:..\lib libcurl_debug.lib
~~~

**(CRTDBG)** Run the application. Heap memory allocations in the CRT are logged
internally. Heap memory leaks are printed to stderr on exit.
~~~cmd
simple
~~~

**(CRTDBG)** Examine the output. If `CURL_FORCEMEMLEAK` was defined then the
last lines of output should show a memory leak in libcurl and the application:
~~~
Detected memory leaks!
Dumping objects ->
x:\j\curl\curl\lib\easy.c(169) : {88} normal block at 0x006A8E38, 7 bytes long.
 Data: <       > CD CD CD CD CD CD CD
x:\j\curl\curl\docs\examples\simple.c(59) : {87} normal block at 0x006E8FB0, 5 bytes long.
 Data: <     > CD CD CD CD CD
Object dump complete.
~~~

**(CURLDEBUG)** Run the application. Heap memory allocations in libcurl are
logged to curldbg.log. Heap memory leaks can be printed by running
memanalyze.pl.
~~~cmd
set "CURL_MEMDEBUG=curldbg.log"
simple
if not exist "%CURL_MEMDEBUG%" echo problem: missing "%CURL_MEMDEBUG%"
..\..\..\tests\memanalyze.pl curldbg.log
~~~

**(CURLDEBUG)** Examine the output. If `CURL_FORCEMEMLEAK` was defined then
memanalyze should show a memory leak in libcurl:
~~~
Leak detected: memory still allocated: 7 bytes
At 4e7a58, there's 7 bytes.
 allocated by x:\j\curl\curl\lib\easy.c:169
~~~
