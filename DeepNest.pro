TEMPLATE = subdirs
SUBDIRS += deepnestlib test_app

test_app.depends = deepnestlib
