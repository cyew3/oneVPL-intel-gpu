{
  'targets': [
    {
      'target_name': 'mfxproxydll',
      'type': 'shared_library',

      'sources': [
        # h:
        'config/config.h',

        'dumps/dump.h',

        'loggers/ilog.h',
        'loggers/log.h',
        'loggers/log_console.h',
        'loggers/log_etw_events.h',
        'loggers/log_file.h',
        'loggers/log_syslog.h',
        'loggers/thread_info.h',
        'loggers/timer.h',

        'wrappers/mfx_core.h',
        'wrappers/mfx_video_core.h',
        'wrappers/mfx_video_decode.h',
        'wrappers/mfx_video_encode.h',
        'wrappers/mfx_video_vpp.h',
        'wrappers/proxymfx.h',

        'tracer/functions_table.h',
        'tracer/bits/mfxfunctions.h',



        # cpp:
        'config/config.cpp',

        'dumps/dump_mfxcommon.cpp',
        'dumps/dump_mfxdefs.cpp',
        'dumps/dump_mfxstructures.cpp',

        'loggers/log.cpp',
        'loggers/log_console.cpp',
        'loggers/log_etw_events.cpp',
        'loggers/log_file.cpp',
        'loggers/log_syslog.cpp',

        'wrappers/mfx_core.cpp',
        'wrappers/mfx_video_core.cpp',
        'wrappers/mfx_video_decode.cpp',
        'wrappers/mfx_video_encode.cpp',
        'wrappers/mfx_video_vpp.cpp',

      ],
      'include_dirs': [
        '.',
        './config',
        './dumps',
        './loggers',
        './wrappers',
        './tracer',
        '../include',
      ],

      'conditions': [
        ['OS == "linux"', {
          'sources': ['tracer/tracer_linux.cpp'],
        }],
        ['OS=="win"', {
          'sources': ['tracer/tracer_windows.cpp'],
        }],
      ],
    },

  ],
}