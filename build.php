<?php
	
//$emscripten = 'c:\\dev\\emscripten';
$emscripten = '/cygdrive/c/dev/emscripten';
$paths = [
	"/usr/bin",
	"{$emscripten}/clang/e1.30.0_64bit",
	"{$emscripten}/node/0.12.2_64bit",
	"{$emscripten}/python/2.7.5.3_64bit",
	"{$emscripten}/emscripten/1.30.0",
	"/cygdrive/c/ProgramData/Oracle/Java/javapath",
	"/cygdrive/c/Users/Carlos/AppData/Roaming/npm",
];

//putenv(getenv('PATH') . implode(':', $paths));
putenv('PATH=' . implode(':', $paths));

$matches = []; preg_match_all('@declare\\s+function\\s+(_\\w+)@', file_get_contents('post.ts'), $matches); $functions = $matches[1];
//$functions[] = '_main';

print_r($paths);
print_r($functions);

$TOTAL_MEMORY = 8 * 1024 * 1024;
passthru('emcc.bat main.c -I ffmpeg -o main.bc');
passthru('tsc -t ES5 post.ts');
$args = [];
$args[] = '-s NO_EXIT_RUNTIME=1';
$args[] = '-s OUTLINING_LIMIT=100000';
$args[] = "-s TOTAL_MEMORY=$TOTAL_MEMORY";
$args[] = '--memory-init-file 0';
$args[] = '-O0';
$args[] = '-s EXPORTED_FUNCTIONS="' . str_replace('"', "'", json_encode($functions)) . '"';
$args[] = 'ffprobe.bc';
$args[] = 'main.bc';
//$args[] = '--pre-js pre.js';
$args[] = '--post-js post.js';
$args[] = '-o ./jspspemu-me.js';
$command = 'emcc.bat ' . implode(' ', $args);
echo "$command\n"; 
passthru($command);


/*

ARGS=%ARGS% -s EXPORTED_FUNCTIONS="['_main', '_main2', '_me_audio_decode_alloc', '_me_audio_decode_set_data', '_me_audio_decode_get_numsamples', '_me_audio_decode_get_numchannels', '_me_audio_decode_get_sample', '_me_audio_decode_free']"
ARGS=%ARGS% -s TOTAL_MEMORY=8000000 -O0 --memory-init-file 0
emcc %ARGS% ffprobe.bc main.bc --pre-js pre.js --post-js post.js -o ./jspspemu-me.js
*/
