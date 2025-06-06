#############################################################################
# Name:        build/cmake/tests/gui/CMakeLists.txt
# Purpose:     CMake file for gui test
# Author:      Tobias Taschner
# Created:     2016-10-31
# Copyright:   (c) 2016 wxWidgets development team
# Licence:     wxWindows licence
#############################################################################

set(TEST_GUI_SRC
    asserthelper.cpp
    test.cpp
    testableframe.cpp
    geometry/rect.cpp
    geometry/size.cpp
    geometry/point.cpp
    geometry/region.cpp
    graphics/bitmap.cpp
    graphics/colour.cpp
    graphics/ellipsization.cpp
    graphics/measuring.cpp
    graphics/affinematrix.cpp
    graphics/boundingbox.cpp
    graphics/clipper.cpp
    graphics/clippingbox.cpp
    graphics/coords.cpp
    graphics/graphbitmap.cpp
    graphics/graphmatrix.cpp
    graphics/graphpath.cpp
    graphics/imagelist.cpp
    config/config.cpp
    controls/auitest.cpp
    controls/bitmapcomboboxtest.cpp
    controls/bitmaptogglebuttontest.cpp
    controls/bookctrlbasetest.cpp
    controls/buttontest.cpp
    controls/checkboxtest.cpp
    controls/checklistboxtest.cpp
    controls/choicebooktest.cpp
    controls/choicetest.cpp
    controls/comboboxtest.cpp
    controls/dataviewctrltest.cpp
    controls/datepickerctrltest.cpp
    controls/frametest.cpp
    controls/gaugetest.cpp
    controls/gridtest.cpp
    controls/headerctrltest.cpp
    controls/htmllboxtest.cpp
    controls/hyperlinkctrltest.cpp
    controls/itemcontainertest.cpp
    controls/infobar.cpp
    controls/label.cpp
    controls/listbasetest.cpp
    controls/listbooktest.cpp
    controls/listboxtest.cpp
    controls/listctrltest.cpp
    controls/listviewtest.cpp
    controls/markuptest.cpp
    controls/notebooktest.cpp
    controls/ownerdrawncomboboxtest.cpp
    controls/pickerbasetest.cpp
    controls/pickertest.cpp
    controls/propgridtest.cpp
    controls/radioboxtest.cpp
    controls/radiobuttontest.cpp
    controls/rearrangelisttest.cpp
    controls/richtextctrltest.cpp
    controls/searchctrltest.cpp
    controls/simplebooktest.cpp
    controls/slidertest.cpp
    controls/spinctrldbltest.cpp
    controls/spinctrltest.cpp
    controls/styledtextctrltest.cpp
    controls/textctrltest.cpp
    controls/textentrytest.cpp
    controls/togglebuttontest.cpp
    controls/toolbooktest.cpp
    controls/treebooktest.cpp
    controls/treectrltest.cpp
    controls/treelistctrltest.cpp
    controls/virtlistctrltest.cpp
    controls/webtest.cpp
    controls/windowtest.cpp
    controls/dialogtest.cpp
    events/clone.cpp
    # Duplicate this file here to test GUI event loops too.
    events/evtlooptest.cpp
    events/propagation.cpp
    events/keyboard.cpp
    # And duplicate this one too as wxExecute behaves differently in
    # console and GUI applications.
    exec/exec.cpp
    font/fonttest.cpp
    image/image.cpp
    image/imagwebp.cpp
    image/rawbmp.cpp
    html/htmlparser.cpp
    html/htmlwindow.cpp
    html/htmprint.cpp
    menu/accelentry.cpp
    menu/menu.cpp
    misc/guifuncs.cpp
    misc/selstoretest.cpp
    misc/garbage.cpp
    misc/safearrayconverttest.cpp
    misc/settings.cpp
    # This one is intentionally duplicated here (it is also part of
    # non-GUI test) as sockets behave differently in console and GUI
    # applications.
    net/socket.cpp
    persistence/tlw.cpp
    persistence/dataview.cpp
    rowheightcache/rowheightcachetest.cpp
    sizers/boxsizer.cpp
    sizers/gridsizer.cpp
    sizers/wrapsizer.cpp
    toplevel/toplevel.cpp
    validators/valnum.cpp
    validators/valtext.cpp
    window/clientsize.cpp
    window/setsize.cpp
    xml/xrctest.cpp

    testprec.h
    testableframe.h
    asserthelper.h
    testdate.h
    testfile.h
    testimage.h
    controls/bookctrlbasetest.h
    controls/itemcontainertest.h
    controls/listbasetest.h
    controls/pickerbasetest.h
    controls/textentrytest.h
    persistence/testpersistence.h
    )

set(TEST_GUI_DATA
    horse.ani
    horse.bmp
    horse.cur
    horse.gif
    horse.ico
    horse.jpg
    horse.pcx
    horse.png
    horse.pnm
    horse.svg
    horse.tga
    horse.tif
    horse.xpm
    horse.webp
    image/bitfields.bmp
    image/bitfields-alpha.bmp
    image/8bpp-colorsused-large.bmp
    image/8bpp-colorsused-negative.bmp
    image/32bpp_rgb.bmp
    image/32bpp_rgb.ico
    image/32bpp_rgb_a0.ico
    image/badrle4.bmp
    image/rgb16-3103.bmp
    image/rgb32-7187.bmp
    image/rgb32bf.bmp
    image/rgba32.bmp
    image/rle4-delta-320x240.bmp
    image/rle8-delta-320x240-expected.bmp
    image/rle8-delta-320x240.bmp
    image/horse_grey.bmp
    image/horse_grey_flipped.bmp
    image/horse_rle4.bmp
    image/horse_rle4_flipped.bmp
    image/horse_rle8.bmp
    image/horse_rle8_flipped.bmp
    image/cross_bicubic_256x256.png
    image/cross_bilinear_256x256.png
    image/cross_box_average_256x256.png
    image/cross_nearest_neighb_256x256.png
    image/horse_bicubic_50x50.png
    image/horse_bicubic_100x100.png
    image/horse_bicubic_150x150.png
    image/horse_bicubic_300x300.png
    image/horse_bilinear_50x50.png
    image/horse_bilinear_100x100.png
    image/horse_bilinear_150x150.png
    image/horse_bilinear_300x300.png
    image/horse_box_average_50x50.png
    image/horse_box_average_100x100.png
    image/horse_box_average_150x150.png
    image/horse_box_average_300x300.png
    image/paste_input_background.png
    image/paste_input_black.png
    image/paste_input_overlay_transparent_border_opaque_square.png
    image/paste_input_overlay_transparent_border_semitransparent_circle.png
    image/paste_input_overlay_transparent_border_semitransparent_square.png
    image/paste_result_background_plus_circle_plus_square.png
    image/paste_result_background_plus_overlay_transparent_border_opaque_square.png
    image/paste_result_background_plus_overlay_transparent_border_semitransparent_square.png
    image/paste_result_no_background_square_over_circle.png
    image/wx.png
    image/toucan.png
    image/toucan_hue_0.538.png
    image/toucan_sat_-0.41.png
    image/toucan_bright_-0.259.png
    image/toucan_hsv_0.538_-0.41_-0.259.png
    image/toucan_light_46.png
    image/toucan_dis_240.png
    image/toucan_grey.png
    image/toucan_mono_255_255_255.png
    image/width-times-height-overflow.bmp
    image/width_height_32_bit_overflow.pgm
    image/bad_truncated.gif
    intl/ja/internat.mo
    intl/ja/internat.po
    )

set(TEST_GUI_RES_BUNDLE
    horse.png
    ../include/wx/msw/bullseye.cur
    )

# On macOS create an .app bundle so the resources can be added.
# On other platforms, just use a console app with gui libraries.
set(APP_TYPE)
if(NOT APPLE)
    set(APP_TYPE CONSOLE_GUI)
endif()

wx_add_test(test_gui ${APP_TYPE} ${TEST_GUI_SRC}
    DATA ${TEST_GUI_DATA}
    RES_BUNDLE ${TEST_GUI_RES_BUNDLE}
    )
if(wxUSE_AUI)
    wx_exe_link_libraries(test_gui wxaui)
endif()
if(wxUSE_RICHTEXT)
    wx_exe_link_libraries(test_gui wxrichtext)
endif()
if(wxUSE_STC)
    wx_exe_link_libraries(test_gui wxstc)
endif()
if(wxUSE_MEDIACTRL)
    wx_exe_link_libraries(test_gui wxmedia)
endif()
if(wxUSE_XRC)
    wx_exe_link_libraries(test_gui wxxrc)
endif()
if(wxUSE_XML)
    wx_exe_link_libraries(test_gui wxxml)
endif()
if(wxUSE_HTML)
    wx_exe_link_libraries(test_gui wxhtml)
endif()
if(wxUSE_SOCKETS)
    wx_exe_link_libraries(test_gui wxnet)
endif()
if(wxUSE_WEBVIEW)
    wx_exe_link_libraries(test_gui wxwebview)
endif()
if(wxUSE_PROPGRID)
    wx_exe_link_libraries(test_gui wxpropgrid)
endif()
