#    Back In Time
#    Copyright (C) 2008 Oprea Dan
#
#    This program is free software; you can redistribute it and/or modify
#    it under the terms of the GNU General Public License as published by
#    the Free Software Foundation; either version 2 of the License, or
#    (at your option) any later version.
#
#    This program is distributed in the hope that it will be useful,
#    but WITHOUT ANY WARRANTY; without even the implied warranty of
#    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#    GNU General Public License for more details.
#
#    You should have received a copy of the GNU General Public License along
#    with this program; if not, write to the Free Software Foundation, Inc.,
#    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.


import os.path
import os
import datetime
import gettext

_=gettext.gettext


gettext.bindtextdomain( 'backintime', '/usr/share/locale' )
gettext.textdomain( 'backintime' )


class Config:
	APP_NAME = 'Back In Time'
	VERSION = '0.7.2'

	NONE = 0
	HOUR = 10
	DAY = 20
	WEEK = 30
	MONTH = 40
	YEAR = 80

	DISK_UNIT_MB = 10
	DISK_UNIT_GB = 20

	AUTOMATIC_BACKUP_MODES = { 
				NONE : _('None'), 
				HOUR : _('Every Hour'), 
				DAY : _('Every Day'), 
				WEEK : _('Every Week'), 
				MONTH : _('Every Month')
				}

	REMOVE_OLD_BACKUP_UNITS = { 
				DAY : _('Day(s)'), 
				WEEK : _('Week(s)'), 
				YEAR : _('Year(s)')
				}

	MIN_FREE_SPACE_UNITS = { DISK_UNIT_MB : 'Mb', DISK_UNIT_GB : 'Gb' }


	def __init__( self ):
		self._APP_PATH = os.path.abspath( os.path.dirname( __file__ ) )

		self._GLOBAL_CONFIG_PATH = '/etc/backintime/config'
		self._CONFIG_FOLDER = os.path.expanduser( '~/.config/backintime' )
		os.system( "mkdir -p \"%s\"" % self._CONFIG_FOLDER )

		self._CONFIG_PATH = os.path.join( self._CONFIG_FOLDER, 'config' )
		self._LOCK_FILE_NAME = 'lock'
		self._PID_FILE = os.path.join( self._CONFIG_FOLDER, 'pid' )

		self._BASE_BACKUP_PATH = ''
		self._INCLUDE_FOLDERS = ''
		self._EXCLUDE_PATTERNS = '.*:*.backup*'
		self._AUTOMATIC_BACKUP = self.NONE
		self._REMOVE_OLD_BACKUPS = 1
		self._REMOVE_OLD_BACKUPS_VALUE = 10
		self._REMOVE_OLD_BACKUPS_UNIT = self.YEAR
		self._MIN_FREE_SPACE = 1
		self._MIN_FREE_SPACE_VALUE = 1
		self._MIN_FREE_SPACE_UNIT = self.DISK_UNIT_GB

		self.MAIN_WINDOW_X = -1
		self.MAIN_WINDOW_Y = -1
		self.MAIN_WINDOW_WIDTH = -1
		self.MAIN_WINDOW_HEIGHT = -1
		self.MAIN_WINDOW_HPANED1_POSITION = -1
		self.MAIN_WINDOW_HPANED2_POSITION = -1
		self.LAST_PATH = ''

		self.load()

	def appPath( self ):
		return self._APP_PATH

	def gladeFile( self ):
		return os.path.join( self.appPath(), 'backintime.glade' )

	def pidFile( self ):
		return self._PID_FILE

	def licenseFile( self ):
		return os.path.join( self.appPath(), 'LICENSE' )

	def licenseText( self ):
		license = ""
		try:
			file = open( self.licenseFile() )
			license = file.read()
			file.close()
		except:
			license = "GPLv2"
		return license

	def backupName( self, date ):
		if type( date ) is datetime.datetime:
			return date.strftime( '%Y%m%d-%H%M%S' )

		if type( date ) is datetime.date:
			return date.strftime( '%Y%m%d-000000' )

		if type( date ) is str:
			return date
		
		return ""

	def backupPath( self, date = None ):
		path = os.path.join( self._BASE_BACKUP_PATH, 'backintime' )

		if date is None:
			return path

		#print "[backupPath] date: %s" % date
		#print "[backupPath] name: %s" % self.backupName( date )
		return os.path.join( path, self.backupName( date ) )

	def backupBasePath( self ):
		return self._BASE_BACKUP_PATH

	def setBackupBasePath( self, value ):
		self._BASE_BACKUP_PATH = value

	def lockFile( self ):
		return os.path.join( self.backupPath(), self._LOCK_FILE_NAME )

	def includeFolders( self ):
		return self._INCLUDE_FOLDERS

	def setIncludeFolders( self, value ):
		self._INCLUDE_FOLDERS = value

	def excludePatterns( self ):
		return self._EXCLUDE_PATTERNS

	def setExcludePatterns( self, value ):
		self._EXCLUDE_PATTERNS = value

	def automaticBackup( self ):
		return self._AUTOMATIC_BACKUP

	def setAutomaticBackup( self, value ):
		self._AUTOMATIC_BACKUP = value
		self.setupCron()

	def removeOldBackups( self ):
		return self._REMOVE_OLD_BACKUPS != 0 and self._REMOVE_OLD_BACKUPS_VALUE > 0

	def removeOldBackupsValue( self ):
		return self._REMOVE_OLD_BACKUPS_VALUE

	def removeOldBackupsUnit( self ):
		return self._REMOVE_OLD_BACKUPS_UNIT

	def setRemoveOldBackups( self, enabled, value, unit ):
		if enabled:
			self._REMOVE_OLD_BACKUPS = 1
		else:
			self._REMOVE_OLD_BACKUPS = 0

		self._REMOVE_OLD_BACKUPS_VALUE = value
		self._REMOVE_OLD_BACKUPS_UNIT = unit

	def removeOldBackupsDate( self ):
		if not self.removeOldBackups():
			return datetime.date( 1, 1, 1 )

		if self._REMOVE_OLD_BACKUPS_UNIT == self.DAY: 
			date = datetime.date.today()
			date = date - datetime.timedelta( days = self._REMOVE_OLD_BACKUPS_VALUE )
			return date

		if self._REMOVE_OLD_BACKUPS_UNIT == self.WEEK:
			date = datetime.date.today()
			date = date - datetime.timedelta( days = 7 * self._REMOVE_OLD_BACKUPS_VALUE )
			return date
		
		if self._REMOVE_OLD_BACKUPS_UNIT == self.YEAR:
			date = datetime.date.today()
			return date.replace( year = date.year - self._REMOVE_OLD_BACKUPS_VALUE )
		
		return datetime.date( 1, 1, 1 )

	def minFreeSpace( self ):
		return self._MIN_FREE_SPACE != 0 and self._MIN_FREE_SPACE_VALUE > 0

	def minFreeSpaceValue( self ):
		return self._MIN_FREE_SPACE_VALUE

	def minFreeSpaceUnit( self ):
		return self._MIN_FREE_SPACE_UNIT

	def setMinFreeSpace( self, enabled, value, unit ):
		if enabled:
			self._MIN_FREE_SPACE = 1
		else:
			self._MIN_FREE_SPACE = 0

		self._MIN_FREE_SPACE_VALUE = value
		self._MIN_FREE_SPACE_UNIT = unit

	def minFreeSpaceValueInMb( self ):
		if not self.minFreeSpace():
			return 0

		value = self.minFreeSpaceValue() #Mb
		if self._MIN_FREE_SPACE_UNIT == self.DISK_UNIT_MB:
			return value

		value *= 1024 #Gb
		if self._MIN_FREE_SPACE_UNIT == self.DISK_UNIT_GB:
			return value

		return 0

	def load( self ):
		configFile = ConfigFile()
		configFile.load( self._GLOBAL_CONFIG_PATH )
		configFile.append( self._CONFIG_PATH )

		self._BASE_BACKUP_PATH = configFile.getString( 'BASE_BACKUP_PATH', self._BASE_BACKUP_PATH )
		self._INCLUDE_FOLDERS = configFile.getString( 'INCLUDE_FOLDERS', self._INCLUDE_FOLDERS )
		self._EXCLUDE_PATTERNS = configFile.getString( 'EXCLUDE_PATTERNS', self._EXCLUDE_PATTERNS )
		self._AUTOMATIC_BACKUP = configFile.getInt( 'AUTOMATIC_BACKUP', self._AUTOMATIC_BACKUP )
		self._REMOVE_OLD_BACKUPS = configFile.getInt( 'REMOVE_OLD_BACKUPS', self._REMOVE_OLD_BACKUPS )
		self._REMOVE_OLD_BACKUPS_VALUE = configFile.getInt( 'REMOVE_OLD_BACKUPS_VALUE', self._REMOVE_OLD_BACKUPS_VALUE )
		self._REMOVE_OLD_BACKUPS_UNIT = configFile.getInt( 'REMOVE_OLD_BACKUPS_UNIT', self._REMOVE_OLD_BACKUPS_UNIT )
		self._MIN_FREE_SPACE = configFile.getInt( 'MIN_FREE_SPACE', self._MIN_FREE_SPACE )
		self._MIN_FREE_SPACE_VALUE = configFile.getInt( 'MIN_FREE_SPACE_VALUE', self._MIN_FREE_SPACE_VALUE )
		self._MIN_FREE_SPACE_UNIT = configFile.getInt( 'MIN_FREE_SPACE_UNIT', self._MIN_FREE_SPACE_UNIT )

		self.MAIN_WINDOW_X = configFile.getInt( 'MAIN_WINDOW_X', -1 )
		self.MAIN_WINDOW_Y = configFile.getInt( 'MAIN_WINDOW_Y', -1 )
		self.MAIN_WINDOW_HEIGHT = configFile.getInt( 'MAIN_WINDOW_HEIGHT', -1 )
		self.MAIN_WINDOW_WIDTH = configFile.getInt( 'MAIN_WINDOW_WIDTH', -1 )
		self.MAIN_WINDOW_HPANED1_POSITION = configFile.getInt( 'MAIN_WINDOW_HPANED1_POSITION', -1 )
		self.MAIN_WINDOW_HPANED2_POSITION = configFile.getInt( 'MAIN_WINDOW_HPANED2_POSITION', -1 )
		self.LAST_PATH = configFile.getString( 'LAST_PATH', '' )

	def save( self ):
		os.system( "mkdir -p \"%s\"" % self._CONFIG_FOLDER )
		configFile = ConfigFile()

		configFile.setString( 'BASE_BACKUP_PATH', self._BASE_BACKUP_PATH )
		configFile.setString( 'INCLUDE_FOLDERS', self._INCLUDE_FOLDERS )
		configFile.setString( 'EXCLUDE_PATTERNS', self._EXCLUDE_PATTERNS )
		configFile.setInt( 'AUTOMATIC_BACKUP', self._AUTOMATIC_BACKUP )
		configFile.setInt( 'REMOVE_OLD_BACKUPS', self._REMOVE_OLD_BACKUPS )
		configFile.setInt( 'REMOVE_OLD_BACKUPS_VALUE', self._REMOVE_OLD_BACKUPS_VALUE )
		configFile.setInt( 'REMOVE_OLD_BACKUPS_UNIT', self._REMOVE_OLD_BACKUPS_UNIT )
		configFile.setInt( 'MIN_FREE_SPACE', self._MIN_FREE_SPACE )
		configFile.setInt( 'MIN_FREE_SPACE_VALUE', self._MIN_FREE_SPACE_VALUE )
		configFile.setInt( 'MIN_FREE_SPACE_UNIT', self._MIN_FREE_SPACE_UNIT )

		configFile.setInt( 'MAIN_WINDOW_X', self.MAIN_WINDOW_X )
		configFile.setInt( 'MAIN_WINDOW_Y', self.MAIN_WINDOW_Y )
		configFile.setInt( 'MAIN_WINDOW_HEIGHT', self.MAIN_WINDOW_HEIGHT )
		configFile.setInt( 'MAIN_WINDOW_WIDTH', self.MAIN_WINDOW_WIDTH )
		configFile.setInt( 'MAIN_WINDOW_HPANED1_POSITION', self.MAIN_WINDOW_HPANED1_POSITION )
		configFile.setInt( 'MAIN_WINDOW_HPANED2_POSITION', self.MAIN_WINDOW_HPANED2_POSITION )
		configFile.setString( 'LAST_PATH', self.LAST_PATH )

		configFile.save( self._CONFIG_PATH )

	def isConfigured( self ):
		if len( self._BASE_BACKUP_PATH ) == 0:
			return False

		if len( self._INCLUDE_FOLDERS ) == 0:
			return False

		return True

	def canBackup( self ):
		if not self.isConfigured():
			return False

		if not os.path.isdir( self._BASE_BACKUP_PATH ):
			return False

		path = self.backupPath()
		os.system( "mkdir -p \"%s\"" % path )
	
		if not os.path.isdir( path ):
			return False

		return True

	def setupCron( self ):
		#remove old cron
		os.system( "crontab -l | grep -v backintime | crontab -" )

		newCron = ""

		if self.automaticBackup() == self.HOUR:
			newCron = "@hourly"
		elif self.automaticBackup() == self.DAY:
			newCron = "@daily"
		elif self.automaticBackup() == self.WEEK:
			newCron = "@weekly"
		elif self.automaticBackup() == self.MONTH:
			newCron = "@monthly"

		if len( newCron ) > 0:
			cmd = " nice -n 19 /usr/bin/backintime --backup"
			newCron = newCron + cmd 
			os.system( "( crontab -l; echo \"%s\" ) | crontab -" % newCron )

class ConfigFile:
	def __init__( self ):
		self.dict = {}

	def save( self, filename ):
		try:
			file = open( filename, 'w' )
			for key, value in self.dict.items():
				file.write( "%s=%s\n" % ( key, value ) )
			file.close()
		except:
			pass

	def load( self, filename ):
		self.dict = {}
		self.append( filename )

	def append( self, filename ):
		lines = []

		try:
			file = open( filename, 'r' )
			lines = file.readlines()
			file.close()
		except:
			pass

		for line in lines:
			items = line.split( '=' )
			if len( items ) == 2:
				self.dict[ items[ 0 ] ] = items[ 1 ][ : -1 ]

	def getString( self, key, defaultValue = '' ):
		try:
			return self.dict[ key ]
		except:
			return defaultValue

	def setString( self, key, value ):
		self.dict[ key ] = value

	def getInt( self, key, defaultValue = 0 ):
		try:
			return int( self.dict[ key ] )
		except:
			return defaultValue

	def setInt( self, key, value ):
		self.dict[ key ] = str( value )


if __name__ == "__main__":
	config = Config()
	print "BASE_BACKUP_PATH = %s" % config._BASE_BACKUP_PATH
	print "INCLUDE_FOLDERS = %s" % config._INCLUDE_FOLDERS
	print "EXCLUDE_PATTERNS = %s" % config._EXCLUDE_PATTERNS
	print "AUTOMATIC_BACKUP = %s" % config._AUTOMATIC_BACKUP
	print "REMOVE_OLD_BACKUPS = %s" % config._REMOVE_OLD_BACKUPS
	print "REMOVE_OLD_BACKUPS_VALUE = %s" % config._REMOVE_OLD_BACKUPS_VALUE
	print "REMOVE_OLD_BACKUPS_UNIT = %s" % config._REMOVE_OLD_BACKUPS_UNIT
	print "MIN_FREE_SPACE = %s" % config._MIN_FREE_SPACE
	print "MIN_FREE_SPACE_VALUE = %s" % config._MIN_FREE_SPACE_VALUE
	print "MIN_FREE_SPACE_UNIT = %s" % config._MIN_FREE_SPACE_UNIT

