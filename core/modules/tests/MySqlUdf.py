#! /usr/bin/env python
import getpass
import math
import optparse
import random
import sys
import unittest
import MySQLdb as sql


def dbparam(x):
    if x is None:
        return 'NULL'
    elif isinstance(x, str):
        return "'" + x + "'"
    else:
        return repr(x)

def angSep(ra1, dec1, ra2, dec2):
    sdt = math.sin(math.radians(ra1 - ra2) * 0.5)
    sdp = math.sin(math.radians(dec1 - dec2) * 0.5)
    cc = math.cos(math.radians(dec1)) * math.cos(math.radians(dec2))
    s = math.sqrt(sdp * sdp + cc * sdt * sdt)
    if s > 1.0:
        return 180.0
    else:
        return 2.0 * math.degrees(math.asin(s))

def ptInSphEllipse(ra, dec, ra_cen, dec_cen, smaa, smia, ang):
    ra = math.radians(ra)
    dec = math.radians(dec)
    v = (math.cos(ra) * math.cos(dec),
         math.sin(ra) * math.cos(dec),
         math.sin(dec))
    theta = math.radians(ra_cen)
    phi = math.radians(dec_cen)
    ang = math.radians(ang)
    sinTheta = math.sin(theta)
    cosTheta = math.cos(theta)
    sinPhi = math.sin(phi)
    cosPhi = math.cos(phi)
    sinAng = math.sin(ang)
    cosAng = math.cos(ang)
    # get coords of input point in (N,E) basis
    n = cosPhi * v[2] - sinPhi * (sinTheta * v[1] + cosTheta * v[0])
    e = cosTheta * v[1] - sinTheta * v[0]
    # rotate by negated major axis angle
    x = sinAng * e + cosAng * n
    y = cosAng * e - sinAng * n
    # scale by inverse of semi-axis-lengths
    x /= math.radians(smaa / 3600.0)
    y /= math.radians(smia / 3600.0)
    # Apply point in circle test for the unit circle centered at the origin
    r = x * x + y * y
    if r < 1.0 - 1e-11:
        return True
    elif r > 1.0 + 1e-11:
        return False
    return None

def flatten(l, ltypes=(list, tuple)):
    ltype = type(l)
    l = list(l)
    i = 0
    while i < len(l):
        while isinstance(l[i], ltypes):
            if not l[i]:
                l.pop(i)
                i -= 1
                break
            else:
                l[i:i + 1] = l[i]
        i += 1
    return ltype(l)


class MySqlUdfTestCase(unittest.TestCase):
    """Tests MySQL UDFs.
    """
    def setUp(self):
        global _options
        self._conn = sql.connect(unix_socket=_options.socketFile,
                                 user=_options.user,
                                 passwd=_options.password)
        self._cursor = self._conn.cursor()

    def tearDown(self):
        self._cursor.close()
        self._conn.close()

    def _query(self, query, result):
        self._cursor.execute(query)
        rows = self._cursor.fetchall()
        self.assertEqual(rows[0][0], result, query +
                         " did not return %s." % dbparam(result))

    def _angSep(self, result, *args):
        args = tuple(map(dbparam, args))
        query = "SELECT qserv_angSep(%s, %s, %s, %s)" % args
        self._cursor.execute(query)
        rows = self._cursor.fetchall()
        if result is None:
            self.assertEqual(rows[0][0], result, query + " did not return NULL.")
        else:
            msg = query + " not close enough to %s" % dbparam(result)
            self.assertAlmostEqual(rows[0][0], result, 11, msg)

    def testAngSep(self):
        for i in xrange(4):
            a = [0.0]*4; a[i] = None
            self._angSep(None, *a)
        for d in (-91.0, 91.0):
            self._angSep(None, 0.0, d, 0.0, 0.0)
            self._angSep(None, 0.0, 0.0, 0.0, d)
        for d in (0.0, 90.0, -90.0):
            self._angSep(0.0, 0.0, d, 0.0, d)
        for i in xrange(100):
            args = [ random.uniform(0.0, 360.0),
                     random.uniform(-90.0, 90.0),
                     random.uniform(0.0, 360.0),
                     random.uniform(-90.0, 90.0) ]
            self._angSep(angSep(*args), *args)

    def _ptInSphBox(self, result, *args):
        args = tuple(map(dbparam, args))
        query = "SELECT qserv_ptInSphBox(%s, %s, %s, %s, %s, %s)" % args
        self._query(query, result)

    def testPtInSphBox(self):
        for i in xrange(6):
            a = [0.0]*6; a[i] = None
            self._ptInSphBox(0, *a)
        for d in (-91.0, 91.0):
            for i in (1, 3, 5):
                a = [0.0]*6; a[i] = d
                self._ptInSphBox(None, *a)
        for ra_min, ra_max in ((370.0, 10.0), (50.0, -90.0), (400.0, -400.0)):
            self._ptInSphBox(None, 0.0, 0.0, ra_min, 0.0, ra_max, 0.0)
        for ra, dec in ((360.0, 0.5), (720.0, 0.5), (5.0, 0.5), (355.0, 0.5)):
            self._ptInSphBox(1, ra, dec, 350.0, 0.0, 370.0, 1.0)
        for ra, dec in ((0.0, 1.1), (0.0, -0.1), (10.1, 0.5), (349.9, 0.5)):
            self._ptInSphBox(0, ra, dec, 350.0, 0.0, 370.0, 1.0)

    def _ptInSphCircle(self, result, *args):
        args = tuple(map(dbparam, args))
        query = "SELECT qserv_ptInSphCircle(%s, %s, %s, %s, %s)" % args
        self._query(query, result)

    def testPtInSphCircle(self):
        for i in xrange(5):
            a = [0.0]*5; a[i] = None
            self._ptInSphCircle(0, *a)
        for d in (-91.0, 91.0):
            self._ptInSphCircle(None, 0.0, d, 0.0, 0.0, 0.0)
            self._ptInSphCircle(None, 0.0, 0.0, 0.0, d, 0.0)
        for r in (-1.0, 181.0):
            self._ptInSphCircle(None, 0.0, 0.0, 0.0, 0.0, r)
        for i in xrange(10):
            ra_cen = random.uniform(0.0, 360.0)
            dec_cen = random.uniform(-90.0, 90.0)
            radius = random.uniform(0.0001, 10.0)
            for j in xrange(100):
                delta = radius / math.cos(math.radians(dec_cen))
                ra = random.uniform(ra_cen - delta, ra_cen + delta)
                dec = random.uniform(max(dec_cen - radius, -90.0),
                                     min(dec_cen + radius, 90.0))
                r = angSep(ra_cen, dec_cen, ra, dec)
                if r < radius - 1e-9:
                    self._ptInSphCircle(1, ra, dec, ra_cen, dec_cen, radius)
                elif r > radius + 1e-9:
                    self._ptInSphCircle(0, ra, dec, ra_cen, dec_cen, radius)

    def _ptInSphEllipse(self, result, *args):
        args = tuple(map(dbparam, args))
        query = "SELECT qserv_ptInSphEllipse(%s, %s, %s, %s, %s, %s, %s)" % args
        self._query(query, result)

    def testPtInSphEllipse(self):
        for i in xrange(7):
            a = [0.0]*7; a[i] = None
            self._ptInSphEllipse(0, *a)
        for d in (-91.0, 91.0):
            self._ptInSphEllipse(None, 0.0, d, 0.0, 0.0, 0.0, 0.0, 0.0)
            self._ptInSphEllipse(None, 0.0, 0.0, 0.0, d, 0.0, 0.0, 0.0)
        self._ptInSphEllipse(None, 0.0, 0.0, 0.0, 0.0, 1.0, 2.0, 0.0)
        self._ptInSphEllipse(None, 0.0, 0.0, 0.0, 0.0, 2.0, -1.0, 0.0)
        self._ptInSphEllipse(None, 0.0, 0.0, 0.0, 0.0, 36001.0, 1.0, 0.0)
        for i in xrange(10):
            ra_cen = random.uniform(0.0, 360.0)
            dec_cen = random.uniform(-90.0, 90.0)
            smaa = random.uniform(0.0001, 36000.0)
            smia = random.uniform(0.00001, smaa)
            ang = random.uniform(-180.0, 180.0)
            for j in xrange(100):
                smaaDeg = smaa / 3600.0
                delta = smaaDeg / math.cos(math.radians(dec_cen))
                ra = random.uniform(ra_cen - delta, ra_cen + delta)
                dec = random.uniform(max(dec_cen - smaaDeg, -90.0),
                                     min(dec_cen + smaaDeg, 90.0))
                r = ptInSphEllipse(ra, dec, ra_cen, dec_cen, smaa, smia, ang)
                if r is True:
                    self._ptInSphEllipse(1, ra, dec, ra_cen, dec_cen, smaa, smia, ang)
                elif r is False:
                    self._ptInSphEllipse(0, ra, dec, ra_cen, dec_cen, smaa, smia, ang)

    def _ptInSphPoly(self, result, *args):
        args = tuple(map(dbparam, args))
        query = "SELECT qserv_ptInSphPoly(%s, %s, %s)" % args
        self._query(query, result)

    def testPtInSphPoly(self):
        for i in xrange(3):
            a = [0.0, 0.0, "0 0 90 0 0 90"]; a[i] = None
            self._ptInSphPoly(0, *a)
        for d in (-91, 91):
            a = [0.0, d, "0 0 90 0 0 90"]
            self._ptInSphPoly(None, *a)
            spec = "0 0 90 0 0 " + str(d)
            a = [0.0, 0.0, spec]
            self.assertRaises(Exception, self._ptInSphPoly,
                              (self, None, a[0], a[1], a[2]))
        self.assertRaises(Exception, self._ptInSphPoly,
                          (self, None, 0.0, 0.0, "0 0 90 0 60 45 30"))
        self.assertRaises(Exception, self._ptInSphPoly,
                          (self, None, 0.0, 0.0, "0 0 90 0 60"))
        self.assertRaises(Exception, self._ptInSphPoly,
                          (self, None, 0.0, 0.0, "0 0 90 0"))
        x = (0, 0);  nx = (180, 0)
        y = (90, 0); ny = (270, 0)
        z = (0, 90); nz = (0, -90)
        tris = [ (x, y, z), (y, nx, z), (nx, ny, z), (ny, (360, 0), z),
                 ((360, 0), ny, nz), (ny, nx, nz), (nx, y, nz), (y, x, nz) ]
        for t in tris:
            spec = " ".join(map(repr, flatten(t)))
            for i in xrange(100):
                ra = random.uniform(0.0, 360.0)
                dec = random.uniform(-90.0, 90.0)
                if ((t[2][1] > 0 and (dec < 0.0 or ra < t[0][0] or ra > t[1][0])) or
                    (t[2][1] < 0 and (dec > 0.0 or ra < t[1][0] or ra > t[0][0]))):
                    self._ptInSphPoly(0, ra, dec, spec)
                else:
                    self._ptInSphPoly(1, ra, dec, spec)


def main():
    global _options

    parser = optparse.OptionParser()
    parser.add_option("-S", "--socket", dest="socketFile",
                      default="/tmp/smm.sock",
                      help="Use socket file FILE to connect to mysql",
                      metavar="FILE")
    parser.add_option("-u", "--user", dest="user",
                      default="qsworker",
                      help="User for db login if not %default")
    parser.add_option("-p", "--password", dest="password",
                      default="",
                      help="Password for db login. ('-' prompts)")
    (_options, args) = parser.parse_args()
    if _options.password == "-":
        _options.password = getpass.getpass()
    random.seed(123456789)
    unittest.main()

if __name__ == "__main__":
    main()

