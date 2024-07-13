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


### Sytem Imports
import time
import random


class Randomization(object): # {
    """
    Class to be used for standard random operations.
    """
    def __init__(self, seed: int=None) -> None: # {
        """
        Constructor for Randomization object.

        Parameters:
            self (Randomization): A Randomization object to be initialized.
            seed (int): The seed value to use to initialize randomization.  The
                integer portion of the current time will be used if no seed is
                provided.

        Returns:
            This method returns no value.
        """
        self.setSeed(seed)
    # }

    def setSeed(self, seed: int=None) -> None: # {
        """
        Set the seed used internally by the object.

        Parameters:
            self (Randomization): A Randomization object to be initialized.
            seed (int): The seed value to use to initialize randomization.  The
                integer portion of the current time will be used if no seed is
                provided.

        Returns:
            This method returns no value.
        """
        if (seed is None):
            seed = int(time.time())
        self.seed = seed
        random.seed(self.seed)
    # }

    def getU64(self, bound: int) -> int: # {
        """
        Get a random number greater-than-or-equal-to 0 and less-than bound.
        As of 2012/06/21, I believe this has up to 53-bits of precision.

        Parameters:
            self (Randomization): A previously-initialized Randomization object.
            bound (int): The upper bound for the return value.  The value
                returned will be strictly less than this value.

        Returns:
            A non-negative integer strictly less than the value of the bound
            parameter.
        """
        if (bound < 1):
            return 0

        return random.randint(0, (bound - 1))
    # }

    def getString(self, length: int=None, alphabet: str=None) -> str: # {
        """
        Get a random string of random characters.

        Parameters:
            self (Randomization): A previously-initialized Randomization object.
            length (int): The number of characters the output string must be.
                Defaults to a randomly-chosen number less than 1024 if omitted.
            alphabet (str): The "alphabet" of characters to choose from when
                constructing the output sting.  Defaults to all printable ASCII
                characters if omitted.

        Returns:
            A string of the specified length containing characters randomly
            chosen from the alphabet.
        """
        if (length is None):
            length = self.getU64(1024) # ARBITRARY

        if (alphabet is None):
            alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz" \
                + "0123456789!@#$%^&*(),./;'[]\\-=`<>?:\"{}|_+~ \t\n"

        output = ""
        for n in range(length):
            output += random.choice(alphabet)

        return output
    # }

    def getHex(self, numBits:int) -> str: # {
        """
        Get a string of a random hexadecimal value that's numBits bits in
        length.

        Parameters:
            self (Randomization): A previously-initialized Randomization object.
            numBits (int): The number of bits that the output hexadecimal value
                must represent.  This parameter is mandatory.

        Returns:
            A string containing the hexadecimal representation of a random value
            strictly less than 2 ** numBits.
        """
        bound = 1 << numBits
        numberValue = self.getU64(bound)

        return hex(numberValue)
    # }

# }

