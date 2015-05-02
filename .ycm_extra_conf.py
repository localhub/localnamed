import os

mypath = os.path.dirname(os.path.abspath(__file__))

def abspath(path):
    return os.path.abspath(os.path.join(mypath, path))

def FlagsForFile(filename, **kwargs):
    return {
        'flags': [
            '-Wall',
            '-Wextra',
            '-Wno-unused-parameter',
            '-Werror',
            '--std=c++11',
            '--stdlib=libc++',
            '-D_XOPEN_SOURCE=1',
            '-I', abspath("include")
        ],
        'do_cache': True
    }
