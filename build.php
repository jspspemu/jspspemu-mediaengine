<?php
	
//$emscripten = 'c:\\dev\\emscripten';
//$emscripten = '/cygdrive/c/dev/emscripten';
$emcc = (PHP_OS == 'CYGWIN') ? "emcc.bat" : "emcc";

if (PHP_OS == 'CYGWIN') {
	$emscripten = realpath(__DIR__ . '/../emscripten');
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
	print_r($paths);
}

$matches = []; preg_match_all('@declare\\s+function\\s+(_\\w+)@', file_get_contents('post.ts'), $matches); $functions = $matches[1];
//$functions[] = '_main';

print_r($functions);

$TOTAL_MEMORY = 16 * 1024 * 1024;
passthru("$emcc main.c -I ffmpeg -o main.bc");
passthru('tsc');
passthru('tsc -m commonjs -t ES5 -d post.ts');
$postdts = file_get_contents('post.d.ts');
$postdts = preg_replace('@\\bdeclare\\b@', '', $postdts);
file_put_contents('../jspspemu/source/src/global/me.d.ts', "declare module MediaEngine {\n{$postdts}\n}");

$args = [];
//$args[] = '-s NO_EXIT_RUNTIME=1';
//$args[] = '-s OUTLINING_LIMIT=100000';
$args[] = "-s TOTAL_MEMORY=$TOTAL_MEMORY";
$args[] = '--memory-init-file 0';
if (PHP_OS == 'CYGWIN') {
	$args[] = '-O0';
} else {
	$args[] = '-O3 -s AGGRESSIVE_VARIABLE_ELIMINATION=1 --closure 1';
}
//$args[] = '-O1';
$args[] = '-s EXPORTED_FUNCTIONS="' . str_replace('"', "'", json_encode($functions)) . '"';
$args[] = 'ffprobe.bc';
$args[] = 'main.bc';

$pre = 'MediaEngine = (function() { exports = (typeof exports != "undefined") ? exports : {}; ';
$post = 'return exports; })();';

file_put_contents('_pre.js', $pre);
file_put_contents('_post.js', file_get_contents('post.js') . "\n" . $post);
unlink('post.js');

$args[] = '--pre-js _pre.js';
$args[] = '--post-js _post.js';
$args[] = '-o ./jspspemu-me.js';
$command = "$emcc " . implode(' ', $args);
echo "$command\n"; 
passthru($command);
unlink('_pre.js');
unlink('_post.js');

copy('jspspemu-me.js', '../jspspemu/jspspemu-me.js');




/*

ARGS=%ARGS% -s EXPORTED_FUNCTIONS="['_main', '_main2', '_me_audio_decode_alloc', '_me_audio_decode_set_data', '_me_audio_decode_get_numsamples', '_me_audio_decode_get_numchannels', '_me_audio_decode_get_sample', '_me_audio_decode_free']"
ARGS=%ARGS% -s TOTAL_MEMORY=8000000 -O0 --memory-init-file 0
emcc %ARGS% ffprobe.bc main.bc --pre-js pre.js --post-js post.js -o ./jspspemu-me.js
*/
