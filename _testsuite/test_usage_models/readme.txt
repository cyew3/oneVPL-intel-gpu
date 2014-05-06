[TARGET]

test_usage_model.exe was designed to test different usage models of MediaSDK.
in the general case, transcoding has DEC + VPP + ENC components.
reference model of transcoding assumes that the same thread call DEC, VPP, ENC and synchronization with async_depth=1.

model 1 assumes that the unique thread/component and syncronization are used. 
(thread_1 call DEC, thread_2 call VPP, thread_3 call ENC and thread_4 call synchronization.

other usage models will be added later.

[USAGE]
example:
test_usage_model.exe -sfmt vc1 -i sa00000.vc1 -dfmt mpeg2 -o out_ref.mpg -w 640 -h 480 -b 3500000 -f 25 -u 1 -async 1 -model 1 -n 100

where
[-sfmt  format]    - format of src video (h264|mpeg2|vc1)
[-dfmt  format]    - format of dst video (h264|mpeg2|vc1)
[-w     width]     - required width  of dst video
[-h     height]    - required height of dst video
[-b     bitRate]   - encoded bit rate (Kbits per second)
[-f     frameRate] - video frame rate (frames per second)
[-u     target]    - target usage (speed|quality|balanced)

[-async depth]     - depth of asynchronous pipeline. default is 1
[-model number]    - number of MediaSDK usage model. default is 1

[-n     frames]    - number of frames to trancode process

[-i     stream]    - src bitstream
[-o     stream]    - dst bitstream


[TEST APPROACH]
1) select target parameters of transcoding (-sfmt, -dfmt, -b, etc)
2) run application with -model 0 and -o out_ref.bs
3) run application with -model 1 and -o out_1.bs
4) metric_calc_lite( out_ref.bs, out_1.bs ) == bitexact

in future, all usage models will be implemented and -model RAND[1, MAX] should be used. 