import os

mypath = os.path.dirname(os.path.abspath(__file__))

def abspath(path):
    return os.path.abspath(os.path.join(mypath, path))


def FlagsForFile(filename, **kwargs):
    return {
        'flags': [
            '-Weverything',
            '-Wno-padded',
            '-I', abspath("include")
        ],
        'do_cache': True
    }
