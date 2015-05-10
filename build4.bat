@echo off
ARGS=
ARGS=%ARGS% -s NO_EXIT_RUNTIME=1 -s OUTLINING_LIMIT=100000
ARGS=%ARGS% -s EXPORTED_FUNCTIONS="['_main', '_main2', '_me_audio_decode_alloc', '_me_audio_decode_set_data', '_me_audio_decode_get_numsamples', '_me_audio_decode_get_numchannels', '_me_audio_decode_get_sample', '_me_audio_decode_free']"
ARGS=%ARGS% -s TOTAL_MEMORY=8000000 -O0 --memory-init-file 0
emcc %ARGS% ffprobe.bc main.bc --pre-js pre.js --post-js post.js -o ./jspspemu-me.js
