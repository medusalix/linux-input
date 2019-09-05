// SPDX-License-Identifier: GPL-2.0+
/*
 * Software Node helpers for the GPIO API
 *
 * Copyright 2022 Google LLC
 */
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/gpio/consumer.h>

#include "gpiolib.h"
#include "gpiolib-swnode.h"

static int swnode_gpiochip_match_name(struct gpio_chip *chip, void *data)
{
	return !strcmp(chip->label, data);
}

struct gpio_desc *swnode_find_gpio(struct fwnode_handle *fwnode,
				   const char *con_id, unsigned int idx,
				   unsigned long *flags)
{
	const struct software_node *chip_node;
	const struct software_node *swnode;
	struct fwnode_reference_args args;
	struct gpio_chip *chip;
	char prop_name[32]; /* 32 is max size of property name */
	int error;

	swnode = to_software_node(fwnode);
	if (!swnode)
		return ERR_PTR(-EINVAL);

	/*
	 * Note we do not need to try both -gpios and -gpio suffixes,
	 * as, unlike OF and ACPI, we can fix software nodes to conform
	 * to the proper binding.
	 */
	if (con_id)
		snprintf(prop_name, sizeof(prop_name), "%s-gpios", con_id);
	else
		strscpy(prop_name, "gpios", sizeof(prop_name));

	/*
	 * We expect all swnode-described GPIOs have GPIO number and
	 * polarity arguments, hence nargs is set to 2.
	 */
	error = fwnode_property_get_reference_args(fwnode, prop_name, NULL,
						   2, idx, &args);
	if (error) {
		pr_debug("%s: can't parse '%s' property of node '%s[%d]'\n",
			__func__, prop_name, swnode->name ?: "unnamed", idx);
		return ERR_PTR(error);
	}

	chip_node = to_software_node(args.fwnode);
	if (!chip_node || !chip_node->name)
		return ERR_PTR(-EINVAL);

	chip = gpiochip_find((void *)chip_node->name,
			     swnode_gpiochip_match_name);
	if (!chip)
		return ERR_PTR(-EPROBE_DEFER);

	/* We expect native GPIO flags */
	*flags = args.args[1];

	return gpiochip_get_desc(chip, args.args[0]);
}

/**
 * swnode_gpio_count - count the GPIOs associated with a device / function
 * @fwnode:	firmware node of the GPIO consumer, can be %NULL for
 * 		system-global GPIOs
 * @con_id:	function within the GPIO consumer
 *
 * Return:
 * The number of GPIOs associated with a device / function or %-ENOENT,
 * if no GPIO has been assigned to the requested function.
 */
int swnode_gpio_count(struct fwnode_handle *fwnode, const char *con_id)
{
	struct fwnode_reference_args args;
	char prop_name[32];
	int count;

	if (con_id)
		snprintf(prop_name, sizeof(prop_name), "%s-gpios", con_id);
	else
		strscpy(prop_name, "gpios", sizeof(prop_name));

	/*
	 * This is not very efficient, but GPIO lists usually have only
	 * 1 or 2 entries.
	 */
	count = 0;
	while (fwnode_property_get_reference_args(fwnode, prop_name, NULL,
						  0, count, &args) == 0)
		count++;

	return count ? count : -ENOENT;
}
