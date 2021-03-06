#!/usr/bin/env python3
# -*- coding: utf-8 -*-

# Python-based Image Tag extractor for Recoll. This is less thorough
# than the Perl-based rclimg script, but useful if you don't want to
# have to install Perl (e.g. on Windows).
#
# Uses pyexiv2. Also tried Pillow, found it useless for tags.
#
from __future__ import print_function
from aip import AipOcr
from aip import AipImageClassify

import sys
import os
import re

import rclexecm

# from filters import rclexecm

try:
    import pyexiv2
except:
    print("RECFILTERROR HELPERNOTFOUND python:pyexiv2")
    sys.exit(1);

khexre = re.compile('.*\.0[xX][0-9a-fA-F]+$')

APP_ID = "15998878"
API_KEY = "qBPgUS6aScH9EtXM8GYwuuRj"
SECRET_KEY = "cSXxA8Ns1TwBLXXyXAvqWGavZYV7gCjZ"


def get_file_content(filePath):  # 这样只要获得文件名就能够进行识别
    with open(filePath, 'rb') as fp:
        return fp.read()


pyexiv2_titles = {
    'Xmp.dc.subject',
    'Xmp.lr.hierarchicalSubject',
    'Xmp.MicrosoftPhoto.LastKeywordXMP',
}

# Keys for which we set meta tags
meta_pyexiv2_keys = {
    'Xmp.dc.subject',
    'Xmp.lr.hierarchicalSubject',
    'Xmp.MicrosoftPhoto.LastKeywordXMP',
    'Xmp.digiKam.TagsList',
    'Exif.Photo.DateTimeDigitized',
    'Exif.Photo.DateTimeOriginal',
    'Exif.Image.DateTime',

    'Exif.Image.Make',
    'Exif.Image.Model'
}

exiv2_dates = ['Exif.Photo.DateTimeOriginal',
               'Exif.Image.DateTime', 'Exif.Photo.DateTimeDigitized']


class ImgTagExtractor:
    def __init__(self, em):
        self.em = em
        self.currentindex = 0
        self._client = AipOcr(APP_ID, API_KEY, SECRET_KEY)
        self._imgClient=AipImageClassify(APP_ID,API_KEY,SECRET_KEY)


    def extractone(self, params):
        # self.em.rclog("extractone %s" % params["filename:"])
        ok = False
        if "filename:" not in params:
            self.em.rclog("extractone: no file name")
            return (ok, docdata, "", rclexecm.RclExecM.eofnow)
        filename = params["filename:"]
        # print("filename ", filename)

        try:
            metadata = pyexiv2.ImageMetadata(filename)
            metadata.read()
            keys = metadata.exif_keys + metadata.iptc_keys + metadata.xmp_keys
            # print("metadata:",keys)
            mdic = {}
            for k in keys:
                # we skip numeric keys and undecoded makernote data
                if k != 'Exif.Photo.MakerNote' and not khexre.match(k):
                    mdic[k] = str(metadata[k].raw_value)
        except Exception as err:
            self.em.rclog("extractone: extract failed: [%s]" % err)
            return (ok, "", "", rclexecm.RclExecM.eofnow)

        docdata = b'<html><head>\n'

        # print("mdic:", mdic)
        ttdata = set()
        for k in pyexiv2_titles:
            if k in mdic:
                ttdata.add(self.em.htmlescape(mdic[k]))
        if ttdata:
            title = ""
            for v in ttdata:
                v = v.replace('[', '').replace(']', '').replace("'", "")
                title += v + " "
            docdata += rclexecm.makebytes("<title>" + title + "</title>\n")

        for k in exiv2_dates:
            if k in mdic:
                # Recoll wants: %Y-%m-%d %H:%M:%S.
                # We get 2014:06:27 14:58:47
                dt = mdic[k].replace(":", "-", 2)
                docdata += b'<meta name="date" content="' + \
                           rclexecm.makebytes(dt) + b'">\n'
                break

        for k, v in mdic.items():
            if k == 'Xmp.digiKam.TagsList':
                docdata += b'<meta name="keywords" content="' + \
                           rclexecm.makebytes(self.em.htmlescape(mdic[k])) + \
                           b'">\n'

        docdata += b'</head><body><pre>\n'
        ## TODO should we need?
        for k, v in mdic.items():
            if k in meta_pyexiv2_keys:
                docdata += rclexecm.makebytes(k + " : " + \
                                          self.em.htmlescape(mdic[k]) + "<br />\n")

        img_result = self._imgClient.advancedGeneral(get_file_content(filename))
        ocr_result = self._client.basicGeneral(get_file_content(filename))
        for item in ocr_result['words_result']:
            docdata += rclexecm.makebytes(self.em.htmlescape(item["words"]) + "<br />\n")
        for item in img_result['result']:
            docdata += rclexecm.makebytes(self.em.htmlescape(item["keyword"]) + "<br />\n")
        docdata += b'</pre></body></html>'

        self.em.setmimetype("text/html")

        return (True, docdata, "", rclexecm.RclExecM.eofnext)

    ###### File type handler api, used by rclexecm ---------->
    def openfile(self, params):
        self.currentindex = 0
        return True

    def getipath(self, params):
        return self.extractone(params)

    def getnext(self, params):
        if self.currentindex >= 1:
            return (False, "", "", rclexecm.RclExecM.eofnow)
        else:
            ret = self.extractone(params)
            self.currentindex += 1
            return ret


if __name__ == '__main__':
    proto = rclexecm.RclExecM()
    extract = ImgTagExtractor(proto)
    rclexecm.main(proto, extract)
