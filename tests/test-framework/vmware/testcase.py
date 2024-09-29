#!/usr/bin/env python
#############################################################################
##
## Copyright (C) 2012 Digia Plc and/or its subsidiary(-ies).
## Contact: http://www.qt-project.org/legal
##
## This file is part of the Qt Installer Framework.
##
## $QT_BEGIN_LICENSE:LGPL$
## Commercial License Usage
## Licensees holding valid commercial Qt licenses may use this file in
## accordance with the commercial license agreement provided with the
## Software or, alternatively, in accordance with the terms contained in
## a written agreement between you and Digia.  For licensing terms and
## conditions see http://qt.digia.com/licensing.  For further information
## use the contact form at http://qt.digia.com/contact-us.
##
## GNU Lesser General Public License Usage
## Alternatively, this file may be used under the terms of the GNU Lesser
## General Public License version 2.1 as published by the Free Software
## Foundation and appearing in the file LICENSE.LGPL included in the
## packaging of this file.  Please review the following information to
## ensure the GNU Lesser General Public License version 2.1 requirements
## will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
##
## In addition, as a special exception, Digia gives you certain additional
## rights.  These rights are described in the Digia Qt LGPL Exception
## version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
##
## GNU General Public License Usage
## Alternatively, this file may be used under the terms of the GNU
## General Public License version 3.0 as published by the Free Software
## Foundation and appearing in the file LICENSE.GPL included in the
## packaging of this file.  Please review the following information to
## ensure the GNU General Public License version 3.0 requirements will be
## met: http://www.gnu.org/copyleft/gpl.html.
##
##
## $QT_END_LICENSE$
##
#############################################################################

import ConfigParser, os, utils

class Step:
    def __init__( self, installscript, checkerTestDir, timeout ):
        self._installscript = installscript
        self._checkerTestDir = checkerTestDir
        self._timeout = timeout
    
    def installscript( self ):
        return self._installscript
    
    def checkerTestDir( self ):
        return self._checkerTestDir
    
    def timeout( self ):
        return self._timeout
    
class TestCase:
    def __init__( self, path ):
        self._steps = []
        config = ConfigParser.SafeConfigParser()
        config.read( path )
        self._path = path
        self._platforms = []
        found = True
        stepNum = 0
        while found:
            sec = "Step{0}".format( stepNum )
            if not config.has_section( sec ):
                found = False
                continue
            
            stepNum += 1
            installscript = utils.makeAbsolutePath( utils.get_config_option( config, None, "installscript", None, sec ), os.path.dirname( path ) )
            checkerTestDir = utils.get_config_option( config, None, "checkerTestDir", None, sec )
            checkerTestDir = utils.makeAbsolutePath( checkerTestDir, os.path.dirname( path ) ) if checkerTestDir else ""
            timeout = int( utils.get_config_option( config, None, "timeout", 60 * 60, sec ) )
            self._steps.append( Step( installscript, checkerTestDir, timeout ) )
             
        self._name = utils.get_config_option( config, None, "name", utils.basename( path ) )
        self._targetDirectory = utils.get_config_option( config, None, "targetDirectory", "" )
        self._maintenanceToolLocation = utils.get_config_option( config, None, "maintenanceToolLocation", "" )
        platforms = utils.get_config_option( config, None, "platforms" )
        if platforms != None:
            self._platforms = platforms.split( ',' )

    def supportsPlatform( self, platform ):
        return platform in self._platforms

    def installerTimeout( self ):
        return self._installerTimeout
    
    def platforms( self ):
        return self._platforms
    
    def name( self ):
        return self._name
    
    def targetDirectory( self ):
        return self._targetDirectory
    
    def steps( self ):
        return self._steps 

    def path( self ):
        return self._path

    def maintenanceToolLocation( self ):
        return self._maintenanceToolLocation

