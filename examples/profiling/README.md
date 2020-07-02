# Profiling measurement tool

Profiling measurement tool using pre-recorded voice files.

## Usage

    nugu_prof [OPTIONS] -i <input-file> -o <output-file>

Options:

     -i <input-file>	Pre-recorded voice file (Raw PCM: 16000Hz, mono, s16le)
     -o <output-file>	File to save reporting results (CSV format)
     -n count		Number of test iterations. (default = 1)
     -m model_path		Set the ASR Model path. (default = /var/lib/nugu/model)
     -d delay		Delay between each test iteration. (seconds, default = 1)
     -t timeout		Timeout for each tests. (seconds, default = 60)

## Result

Result(CSV format, `-n 4` option) example:

    Num,End detected,ASR Result,TTS Speak directive,TTS 1st data,TTS 1st pcm write
    1,1858,148,559,62,1
    2,1852,144,448,60,1
    3,1854,142,374,69,1
    4,1850,144,472,63,1

If you open the result file using CSV viewer program, you can see the table below:

|Num|End detected|ASR Result|TTS Speak directive|TTS 1st data|TTS 1st pcm write|
|:-:|:----------:|:--------:|:-----------------:|:----------:|:---------------:|
|1|1858|148|559|62|1|
|2|1852|144|448|60|1|
|3|1854|142|374|69|1|
|4|1850|144|472|63|1|

## Pre-recorded voice file

PCM files written in the following format are used.

* Sample rate: **16000Hz**
* Encoding: **s16le** (Sample size: 16 bits, signed-integer, little-endian)
* Channel: **1** (mono)

### How to generate a pre-recorded voice file using a text string (TTS dump to a file)

Setting the PCM file output directory through setting environment variables.

    export NUGU_DUMP_PATH_PCM=/tmp

Run the TTS sample app using the string you want to generate PCM output. (**22050Hz**, **s16le**, **mono**)

    nugu_simple_tts "며칠이야"
    nugu_simple_tts "몇시야"
    nugu_simple_tts "4 더하기 4 는 얼마야?"
    nugu_simple_tts "날씨"
    nugu_simple_tts "Flo 들려줘"

Check the PCM file creation. (file name: `papcm_{date}_{time}.dat`)

    ls /tmp
    papcm_20200629_044609.dat

### Convert the PCM sample rate 22050Hz to 16000Hz

You can easily convert the sample rate through the `sox` program.

    sudo apt install sox
    sox -t raw -r 22050 -e signed-integer -L -b 16 -c 1 /tmp/papcm_20200629_044609.dat date.raw rate 16000

Finally, you created a `date.raw` file converted to 16000Hz which can be used for profiling tools.
