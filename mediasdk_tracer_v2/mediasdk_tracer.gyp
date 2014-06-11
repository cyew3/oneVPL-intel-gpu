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
    'standalone_static_library': '1',
    'default_configuration': 'Debug_x64',
    'configurations': {
      'Debug_Win32': {
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
      },
      'Debug_x64': {
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
      },
      'Release_Win32': {
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
      },
      'Release_x64': {
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
      },
    },
    'include_dirs': [
      '.',
      './config',
      './dumps',
      './loggers',
      './wrappers',
      './tracer',
      '../include',
    ],
  },

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
  {
    'target_name': 'mfx-tracer-config',
    'type': 'executable',
    'sources': [
      'strfuncs.h',
      'config_manager.h',
      'config_manager.cpp',
      'main.cpp',
    ],
  },],
}