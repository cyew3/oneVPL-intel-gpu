{
  'target_defaults': {
    'msbuild_configuration_attributes': {
      'CharacterSet': 'Unicode',
      'IntermediateDirectory': 'build\\win_$(Platform)\\objs\\$(ProjectName)\\',
      'OutputDirectory': 'build/lib/$(Platform)',
    },
    'msvs_settings': {
      'VCCLCompilerTool': {
        'BasicRuntimeChecks': '3',            # EnableFastChecks
        'ExceptionHandling': '2',             # Async
        'MinimalRebuild': 'true',
        'WarningLevel': '4',
        'WholeProgramOptimization': 'false',
      },
      'VCLibrarianTool': {
        'OutputFile': '$(OutDir)$(TargetName)$(TargetExt)',
      },
    },
    'default_configuration': 'Debug_x64',
    'configurations': {
      'Debug_Win32': {
        'conditions': [
          ['OS == "linux"', {
            'cflags': ['-g', '-O0', '-fPIC'],
          }],
          ['OS=="win"', {
            'msvs_configuration_platform': 'Win32',
            'defines': [ 'DEBUG', '_DEBUG', 'WIN32', '_LIB' ],
            'msbuild_configuration_attributes': {
              'TargetName': '$(ProjectName)_d',
            },
            'msvs_settings': {
              'VCCLCompilerTool': {
                'DebugInformationFormat': '3',        # ProgramDatabase
                'Optimization': '0',                  # Disabled
                'RuntimeLibrary': '1',                # MultiThreadedDebug
                'WarnAsError': 'false',
              },
              'VCLinkerTool': {
                'LinkTimeCodeGeneration': 1,
                'OptimizeReferences': 2,
                'EnableCOMDATFolding': 2,
                'LinkIncremental': 1,
                'GenerateDebugInformation': 'true',
              },
            },
          }],
        ], 
      },
      'Debug_x64': {
        'conditions': [
          ['OS == "linux"', {
            'cflags': ['-g', '-O0', '-fPIC'],
          }],
          ['OS=="win"', {
            'msvs_configuration_platform': 'x64',
            'defines': [ 'DEBUG', '_DEBUG', 'WIN32', 'WIN64', '_LIB' ],
            'msvs_settings': {
              'VCCLCompilerTool': {
                'DebugInformationFormat': '3',        # ProgramDatabase
                'Optimization': '0',                  # Disabled
                'RuntimeLibrary': '1',                # MultiThreadedDebug
                'WarnAsError': 'false',
              },
              'VCLinkerTool': {
                'LinkTimeCodeGeneration': 1,
                'OptimizeReferences': 2,
                'EnableCOMDATFolding': 2,
                'LinkIncremental': 1,
                'GenerateDebugInformation': 'true',
              },
            },
            'msbuild_configuration_attributes': {
              'TargetName': '$(ProjectName)_d',
            },
          }],
        ],  
      },
      'Release_Win32': {
        'conditions': [
          ['OS == "linux"', {
            'cflags': ['-O2', '-fPIC'],
          }],
          ['OS=="win"', {
            'msvs_configuration_platform': 'Win32',
            'defines': [ 'NDEBUG', 'WIN32', '_LIB' ],
            'msvs_settings': {
              'VCCLCompilerTool': {
                'DebugInformationFormat': '3',        # ProgramDatabase
                'Optimization': '0',                  # Disabled
                'RuntimeLibrary': '0',                # MultiThreaded
                'WarnAsError': 'false',
              },
            },
            'msbuild_configuration_attributes': {
              'TargetName': '$(ProjectName)',
            },
          }],
        ],
      },
      'Release_x64': {
        'conditions': [
          ['OS == "linux"', {
            'cflags': ['-O2', '-fPIC'],
          }],
          ['OS=="win"', {
            'msvs_configuration_platform': 'x64',
            'defines': [ 'NDEBUG', 'WIN32', 'WIN64', '_LIB' ],
            'msvs_settings': {
              'VCCLCompilerTool': {
                'DebugInformationFormat': '3',        # ProgramDatabase
                'Optimization': '0',                  # Disabled
                'RuntimeLibrary': '0',                # MultiThreaded
                'WarnAsError': 'false',
              },
            },
            'msbuild_configuration_attributes': {
              'TargetName': '$(ProjectName)',
            },
          }],
        ],
      },
    },
  },

  'targets': [
  {
    'target_name': 'mfx-tracer',
    'type': 'shared_library',
    'msvs_settings': {
      'VCLinkerTool': {
        'ModuleDefinitionFile': 'mfx-tracer.def',
      },
    },
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

      'tracer/tracer.h',
      'tracer/functions_table.h',
      'tracer/bits/mfxfunctions.h',


      # cpp:
      'config/config.cpp',

      'dumps/dump.cpp',
      'dumps/dump_mfxcommon.cpp',
      'dumps/dump_mfxdefs.cpp',
      'dumps/dump_mfxenc.cpp',
      'dumps/dump_mfxplugin.cpp',
      'dumps/dump_mfxsession.cpp',
      'dumps/dump_mfxstructures.cpp',
      'dumps/dump_mfxvideo.cpp',

      'loggers/log.cpp',
      'loggers/log_console.cpp',
      'loggers/log_etw_events.cpp',
      'loggers/log_file.cpp',
      'loggers/log_syslog.cpp',

      'wrappers/mfx_core.cpp',
      'wrappers/mfx_video_core.cpp',
      'wrappers/mfx_video_decode.cpp',
      'wrappers/mfx_video_enc.cpp',
      'wrappers/mfx_video_encode.cpp',
      'wrappers/mfx_video_user.cpp',
      'wrappers/mfx_video_vpp.cpp',

       'tracer/tracer.cpp',
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
        'cflags': ['-fPIC'], 
      }],
      ['OS=="win"', {
        'sources': ['tracer/tracer_windows.cpp'],
      }],
    ],
  },
  {
    'target_name': 'mfx-tracer-config',
    'type': 'executable',
    'sources': [
      'tools/configure/strfuncs.h',
      'tools/configure/config_manager.h',
      'tools/configure/config_manager.cpp',
      'tools/configure/main.cpp',
    ],
  },],
}