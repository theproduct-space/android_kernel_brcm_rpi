#
# Makefile for the NXP PN7160 NFC driver
#

# Define the object files needed for the I2C version of the driver.
# This creates a composite object named 'nxpnfc_i2c.o' from two source files.
nxpnfc_i2c-objs   := i2c_drv.o common.o

# Define the object files needed for the SPI version.
nxpnfc_spi-objs   := spi_drv.o common.o

# Add the final driver objects to the build if their Kconfig option is enabled.
obj-$(CONFIG_NXP_NFC_I2C)   += nxpnfc_i2c.o
obj-$(CONFIG_NXP_NFC_SPI)   += nxpnfc_spi.o