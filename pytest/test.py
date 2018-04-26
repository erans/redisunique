import unittest
from rmtest import ModuleTestCase
import os

module_path = os.path.join(os.path.dirname(os.path.realpath(__file__)), '../redisuniquemodule.so')

class SnowflakeTestCase(ModuleTestCase(module_path, module_args=('snowflake_region=1','snowflake_worker=1'))):

    def testSnowflake(self):
        with self.redis() as r:
            v = r.execute_command('uniqueid.snowflake')
            print(v)
            self.assertIsNotNone(v)

    def testUUIDv1(self):
        with self.redis() as r:
            v = r.execute_command('uniqueid.uuidv1')
            self.assertIsNotNone(v)

    def testUUIDv4(self):
        with self.redis() as r:
            v = r.execute_command('uniqueid.uuidv4')
            self.assertIsNotNone(v)


    def testCommandProxy(self):
        with self.redis() as r:
            v = r.execute_command('uniqueid.proxy', 'set x $snowflake$')

if __name__ == '__main__':
    unittest.main()
