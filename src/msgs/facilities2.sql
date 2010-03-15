/* MAX_NUMBER is the next number to be used, always one more than the highest message number. */
set bulk_insert INSERT INTO FACILITIES (LAST_CHANGE, FACILITY, FAC_CODE, MAX_NUMBER) VALUES (?, ?, ?, ?);
--
('2010-02-25 12:40:00', 'JRD', 0, 700)
('2010-03-15 06:59:09', 'QLI', 1, 531)
--
--('2008-11-28 20:27:04', 'GDEF', 2, 346)
--
('2009-07-16 05:26:11', 'GFIX', 3, 121)
('1996-11-07 13:39:40', 'GPRE', 4, 1)
--
--('1996-11-07 13:39:40', 'GLTJ', 5, 1)
--('1996-11-07 13:39:40', 'GRST', 6, 1)
--
('2005-11-05 13:09:00', 'DSQL', 7, 32)
('2010-01-13 15:04:00', 'DYN', 8, 278)
--
--('1996-11-07 13:39:40', 'FRED', 9, 1)
--
('1996-11-07 13:39:40', 'INSTALL', 10, 1)
('1996-11-07 13:38:41', 'TEST', 11, 4)
('2009-01-03 09:06:33', 'GBAK', 12, 350)
('2009-06-05 23:07:00', 'SQLERR', 13, 970)
('1996-11-07 13:38:42', 'SQLWARN', 14, 613)
('2006-09-10 03:04:31', 'JRD_BUGCHK', 15, 307)
--
--('1996-11-07 13:38:43', 'GJRN', 16, 241)
--
('2009-12-21 04:00:05', 'ISQL', 17, 173)
('2009-11-13 17:49:54', 'GSEC', 18, 104)
--
--('2002-03-05 02:30:12', 'LICENSE', 19, 60)
--('2002-03-05 02:31:54', 'DOS', 20, 74)
--
('2009-12-26 14:22:00', 'GSTAT', 21, 50)
('2009-12-18 19:33:34', 'FBSVCMGR', 22, 57)
('2009-07-18 12:12:12', 'UTL', 23, 2)
('2009-12-19 06:19:15', 'NBACKUP', 24, 73)
('2009-07-20 07:55:48', 'FBTRACEMGR', 25, 41)
stop

COMMIT WORK;
