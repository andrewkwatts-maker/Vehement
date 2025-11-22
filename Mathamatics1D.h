#pragma once

static int CapInt(int Value, int min, int max)
{
	if (Value < min)
		Value = min;
	if (Value > max)
		Value = max;
	return Value;
}