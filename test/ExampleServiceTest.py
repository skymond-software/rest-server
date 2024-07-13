#!/usr/bin/env python
################################################################################
##                                                                            ##
##            (c) Copyright 2022 James Card. All Rights Reserved.             ##
##                                                                            ##
##                         https://www.jamescard.org                          ##
##                                                                            ##
## Permission is hereby granted, free of charge, to any person obtaining a    ##
## copy of this software and associated documentation files (the "Software"), ##
## to deal in the Software without restriction, including without limitation  ##
## the rights to use, copy, modify, merge, publish, distribute, sublicense,   ##
## and#or sell copies of the Software, and to permit persons to whom the      ##
## Software is furnished to do so, subject to the following conditions:       ##
##                                                                            ##
## The above copyright notice and this permission notice shall be included    ##
## in all copies or substantial portions of the Software.                     ##
##                                                                            ##
## THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR ##
## IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,   ##
## FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL    ##
## THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER ##
## LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING    ##
## FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER        ##
## DEALINGS IN THE SOFTWARE.                                                  ##
##                                                                            ##
################################################################################


from WebServiceClient import WebServiceClient

class TestCase(WebServiceClient):
    """
    Test case for Example web service.
    """
    def __init__(self): # {
        """
        Constructor for test case.

        Parameters:
            self (TestCase): A TestCase object being initialzied.

        Returns:
            This method returns no value.
        """
        super().__init__("localhost:9000", "webService")
    # }

    def goodLogin(self): # {
        """
        Use the login method with valid parameters.

        Parameters:
            self (TestCase): A previously-initialized TestCase object.

        Returns:
            This function returns no value.
        """
        loginResponse = self.login("user", "user")
        if (loginResponse.type != "goodLoginResponse"):
            self.logger.err(f"Server returned status {loginResponse.status}")
            self.term(self.STATUS_FAIL, f"Expected goodLoginResponse, got {loginResponse.type}")
    # }

    def badLogin(self): # {
        """
        Use the login method with invalid parameters.

        Parameters:
            self (TestCase): A previously-initialized TestCase object.

        Returns:
            This function returns no value.
        """
        loginResponse = self.login(self.rand.getString(), self.rand.getString())
        if (loginResponse.type != "badLoginResponse"):
            self.logger.err(f"Server returned status {loginResponse.status}")
            self.term(self.STATUS_FAIL, f"Expected badLoginResponse, got {loginResponse.type}")
    # }

    def badLogout(self): # {
        """
        Use the logout method with invalid parameters.

        Parameters:
            self (TestCase): A previously-initialized TestCase object.

        Returns:
            This function returns no value.
        """
        logoutResponse = self.logout(self.rand.getU64(65536))
        if (logoutResponse.type != "badLogoutResponse"):
            self.logger.err(f"Server returned status {logoutResponse.status}")
            self.term(self.STATUS_FAIL, f"Expected badLoginResponse, got {logoutResponse.type}")

        if (self.sessionToken is None):
            self.term(self.STATUS_FAIL,
                "Failed logout call cleared session token.")
    # }

    def goodLogout(self): # {
        """
        Use the logout method with valid parameters.

        Parameters:
            self (TestCase): A previously-initialized TestCase object.

        Returns:
            This function returns no value.
        """
        # The session token should only be cached when there's a successful
        # login.  The first attempt was good, so that should have cached a valid
        # token.  The second attempt should have failed, so it shouldn't have
        # cached anything but the token from the first attempt should still be
        # valid.
        logoutResponse = self.logout()
        if (logoutResponse.type != "goodLogoutResponse"):
            self.logger.err(f"Server returned status {logoutResponse.status}")
            self.term(self.STATUS_FAIL, f"Expected goodLoginResponse, got {logoutResponse.type}")

        if (self.sessionToken is not None):
            self.term(self.STATUS_FAIL,
                "Successful logout call did not clear session token.")
    # }

    def main(self): # {
        """
        Main driver for the test case.

        Parameters:
            self (TestCase): A previously-initialized TestCase object.

        Returns:
            0 on success.
        """
        self.logger.step("Using the login method correctly.")
        self.goodLogin()

        self.logger.step("Using the login method incorrectly.")
        self.badLogin()

        self.logger.step("Using the logout method incorrectly.")
        self.badLogout()

        self.logger.step("Using the logout method correctly.")
        self.goodLogout()

        return 0
    # }

if (__name__ == "__main__"):
    testCase = TestCase()
    exit(testCase.run())

