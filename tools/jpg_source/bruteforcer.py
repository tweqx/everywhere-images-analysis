# Solves for the luminance quantization matrix Q given the equations generated by ./jpg_source

from math import ceil, floor

def do_quantize(unquantized_dct, divisor):
  dct = unquantized_dct

  if dct < 0:
    dct = -dct

    dct += divisor >>1

    if dct >= divisor:
      dct //= divisor
    else:
      dct = 0

    dct = -dct
  else:
    dct += divisor >>1

    if dct >= divisor:
      dct //= divisor
    else:
      dct = 0

  return dct

def do_unquantize(quantized_dct, divisor):
  if quantized_dct == 0:
    return list(range(-ceil(divisor / 2) + 1, ceil(divisor / 2)))
  elif quantized_dct < 0:
    quantized_dct = -quantized_dct

    starting_offset = quantized_dct * divisor - (divisor >> 1)

    return list(range(-starting_offset, -starting_offset - divisor, -1))
  else:
    starting_offset = quantized_dct * divisor - (divisor >> 1)
    return list(range(starting_offset, starting_offset + divisor))

def find_divisors(source, output):
  divisors = []

  source = abs(source)
  output = abs(output)

  # q = 2k
  lower_bound = ceil(source/(output + 0.5))
  upper_bound = floor(source/(output - 0.5))

  for q in range(lower_bound, upper_bound + 1):
    if q != 0 and q % 2 == 0 and do_quantize(source, q) == output:
      divisors.append(q)

  # q = 2k + 1
  lower_bound = ceil(2 * (source - 1 - output) / (2*output + 1) + 1) 
  upper_bound = floor(2 * (source - output)/(2*output - 1) + 1)

  for q in range(lower_bound, upper_bound + 1):
    if q % 2 == 1 and do_quantize(source, q) == output:
      divisors.append(q)

  return divisors

equations = { i: [] for i in range(64) }

with open("./equations", "r") as f:
  for line in f:
    # q(q(s, Qu) * Qu, d75) \\in {v1, v2}
    s, u, d75, v1, v2 = list(map(int, line.split())) 

    equations[u].append((s, d75, v1, v2))

def solve_coeff(eq_list):
  candidates_distribution = { i: 0 for i in range(1, 99 + 1) }

  for eq in eq_list:
    s, d75, v1, v2 = eq

    # q(q(s, Qu) * Qu, d75) \in {v1, v2}

    intermediates = do_unquantize(v1, d75) + do_unquantize(v2, d75)
    # q(s, Qu) * Qu \in intermediates

    #print(s, intermediates)

    eq_candidates = set()
    for Qu in range(1, 99 + 1):
      if do_quantize(s, Qu) * Qu in intermediates:
        eq_candidates.add(Qu)

    for candidate in eq_candidates:
      candidates_distribution[candidate] += 1

  total_count = sum([ candidates_distribution[i] for i in range(1, 99 + 1) ])
  for i in range(1, 99 + 1):
    candidates_distribution[i] /= total_count

  return sorted(list(range(1, 99 + 1)), key=lambda i: -candidates_distribution[i])

for coeff, eq_list in equations.items():
  print(coeff, solve_coeff(eq_list))
