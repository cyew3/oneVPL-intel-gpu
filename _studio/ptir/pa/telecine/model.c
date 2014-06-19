/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2014 Intel Corporation. All Rights Reserved.

File Name: model.c

\* ****************************************************************************** */

#include "model.h"

unsigned int __stdcall classify(double dTextureLevel, double dDynDif, double dStatDif, double dStatCount, double dCountDif, double dZeroTexture, double dRsT, double dAngle, double dSADv, double dBigTexture, double dCount, double dRsG, double dRsDif, double dRsB, double SADCBPT, double SADCTPB)
{
	if (dCount <= 65)
	{
		if (SADCBPT <= 3.963)
		{
			if (dCountDif <= 0.276)
			{
				if (SADCTPB <= 5.039)
				{
					if (dRsG <= 9.563)
					{
						if (dCountDif <= 0.199)
						{
							if (SADCTPB <= 2.995)
							{
								if (dSADv <= 2.986)
								{
									if (dRsG <= 7.231)
									{
										return 0;
									}
									else if (dRsG > 7.231)
									{
										if (dTextureLevel <= -5.1305)
										{
											if (dRsG <= 7.387)
											{
												return 0;
											}
											else if (dRsG > 7.387)
											{
												if (dRsG <= 7.423)
												{
													return 1;
												}
												else if (dRsG > 7.423)
												{
													return 0;
												}
											}
										}
										else if (dTextureLevel > -5.1305)
										{
											return 1;
										}
									}
								}
								else if (dSADv > 2.986)
								{
									if (dTextureLevel <= -5.227)
									{
										return 0;
									}
									else if (dTextureLevel > -5.227)
									{
										return 1;
									}
								}
							}
							else if (SADCTPB > 2.995)
							{
								return 0;
							}
						}
						else if (dCountDif > 0.199)
						{
							if (dSADv <= 2.84)
							{
								return 1;
							}
							else if (dSADv > 2.84)
							{
								if (dTextureLevel <= -4.0655)
								{
									if (dBigTexture <= 0.852)
									{
										if (dTextureLevel <= -4.1205)
										{
											return 0;
										}
										else if (dTextureLevel > -4.1205)
										{
											if (dBigTexture <= 0.68)
											{
												return 0;
											}
											else if (dBigTexture > 0.68)
											{
												return 1;
											}
										}
									}
									else if (dBigTexture > 0.852)
									{
										if (dSADv <= 2.973)
										{
											if (dRsG <= 7.365)
											{
												return 0;
											}
											else if (dRsG > 7.365)
											{
												return 1;
											}
										}
										else if (dSADv > 2.973)
										{
											return 1;
										}
									}
								}
								else if (dTextureLevel > -4.0655)
								{
									return 1;
								}
							}
						}
					}
					else if (dRsG > 9.563)
					{
						if (dSADv <= 4.108)
						{
							return 1;
						}
						else if (dSADv > 4.108)
						{
							if (dTextureLevel <= -3.9265)
							{
								return 0;
							}
							else if (dTextureLevel > -3.9265)
							{
								if (dRsDif <= 9.677)
								{
									return 1;
								}
								else if (dRsDif > 9.677)
								{
									return 0;
								}
							}
						}
					}
				}
				else if (SADCTPB > 5.039)
				{
					return 0;
				}
			}
			else if (dCountDif > 0.276)
			{
				if (dStatCount <= 23)
				{
					if (dAngle <= 0.102)
					{
						if (dRsG <= 7.227)
						{
							if (dSADv <= 2.861)
							{
								return 1;
							}
							else if (dSADv > 2.861)
							{
								return 0;
							}
						}
						else if (dRsG > 7.227)
						{
							return 1;
						}
					}
					else if (dAngle > 0.102)
					{
						if (dTextureLevel <= -4.0295)
						{
							return 0;
						}
						else if (dTextureLevel > -4.0295)
						{
							return 1;
						}
					}
				}
				else if (dStatCount > 23)
				{
					return 0;
				}
			}
		}
		else if (SADCBPT > 3.963)
		{
			if (dStatCount <= 55)
			{
				if (SADCBPT <= 23.268)
				{
					if (dZeroTexture <= 0.148)
					{
						if (dCount <= 59)
						{
							return 0;
						}
						else if (dCount > 59)
						{
							if (SADCTPB <= 3.726)
							{
								if (dCount <= 64)
								{
									return 1;
								}
								else if (dCount > 64)
								{
									return 0;
								}
							}
							else if (SADCTPB > 3.726)
							{
								if (dRsG <= 13.267)
								{
									return 0;
								}
								else if (dRsG > 13.267)
								{
									if (dSADv <= 6.232)
									{
										if (dTextureLevel <= -9.174)
										{
											return 0;
										}
										else if (dTextureLevel > -9.174)
										{
											return 1;
										}
									}
									else if (dSADv > 6.232)
									{
										return 0;
									}
								}
							}
						}
					}
					else if (dZeroTexture > 0.148)
					{
						if (dSADv <= 4.788)
						{
							if (dBigTexture <= 0.587)
							{
								if (dStatCount <= 38)
								{
									if (dRsB <= 6.229)
									{
										return 0;
									}
									else if (dRsB > 6.229)
									{
										return 1;
									}
								}
								else if (dStatCount > 38)
								{
									return 0;
								}
							}
							else if (dBigTexture > 0.587)
							{
								if (dZeroTexture <= 0.635)
								{
									if (dRsG <= 10.159)
									{
										return 0;
									}
									else if (dRsG > 10.159)
									{
										if (dSADv <= 4.229)
										{
											return 1;
										}
										else if (dSADv > 4.229)
										{
											if (dZeroTexture <= 0.212)
											{
												if (dStatCount <= 49)
												{
													return 0;
												}
												else if (dStatCount > 49)
												{
													return 1;
												}
											}
											else if (dZeroTexture > 0.212)
											{
												return 0;
											}
										}
									}
								}
								else if (dZeroTexture > 0.635)
								{
									return 0;
								}
							}
						}
						else if (dSADv > 4.788)
						{
							if (dSADv <= 4.992)
							{
								return 0;
							}
							else if (dSADv > 4.992)
							{
								return 1;
							}
						}
					}
				}
				else if (SADCBPT > 23.268)
				{
					if (dTextureLevel <= -1.8325)
					{
						return 0;
					}
					else if (dTextureLevel > -1.8325)
					{
						return 1;
					}
				}
			}
			else if (dStatCount > 55)
			{
				if (SADCTPB <= 4.855)
				{
					if (dSADv <= 4.906)
					{
						if (SADCBPT <= 4.84)
						{
							if (dTextureLevel <= -3.9945)
							{
								if (dZeroTexture <= 0.419)
								{
									if (dSADv <= 4.561)
									{
										return 1;
									}
									else if (dSADv > 4.561)
									{
										return 0;
									}
								}
								else if (dZeroTexture > 0.419)
								{
									return 0;
								}
							}
							else if (dTextureLevel > -3.9945)
							{
								if (dBigTexture <= 0.835)
								{
									return 0;
								}
								else if (dBigTexture > 0.835)
								{
									return 1;
								}
							}
						}
						else if (SADCBPT > 4.84)
						{
							return 0;
						}
					}
					else if (dSADv > 4.906)
					{
						if (dBigTexture <= 2.239)
						{
							return 1;
						}
						else if (dBigTexture > 2.239)
						{
							return 0;
						}
					}
				}
				else if (SADCTPB > 4.855)
				{
					if (dRsG <= 6.079)
					{
						if (dZeroTexture <= 0.014)
						{
							return 0;
						}
						else if (dZeroTexture > 0.014)
						{
							return 1;
						}
					}
					else if (dRsG > 6.079)
					{
						return 0;
					}
				}
			}
		}
	}
	else if (dCount > 65)
	{
		if (SADCTPB <= 5.02)
		{
			if (dZeroTexture <= 2.884)
			{
				if (dBigTexture <= 0.153)
				{
					if (dStatCount <= 35)
					{
						return 0;
					}
					else if (dStatCount > 35)
					{
						if (dCount <= 289)
						{
							return 0;
						}
						else if (dCount > 289)
						{
							return 1;
						}
					}
				}
				else if (dBigTexture > 0.153)
				{
					if (dCount <= 529)
					{
						if (dRsB <= 18.334)
						{
							if (dTextureLevel <= -3.802)
							{
								if (dSADv <= 5.021)
								{
									if (SADCBPT <= 5.08)
									{
										if (dDynDif <= 0.108)
										{
											return 0;
										}
										else if (dDynDif > 0.108)
										{
											if (dTextureLevel <= -6.2005)
											{
												if (dRsDif <= 8.936)
												{
													return 0;
												}
												else if (dRsDif > 8.936)
												{
													return 1;
												}
											}
											else if (dTextureLevel > -6.2005)
											{
												if (dRsB <= 16.209)
												{
													if (dSADv <= 3.642)
													{
														if (dAngle <= 0.016)
														{
															if (dCount <= 82)
															{
																return 0;
															}
															else if (dCount > 82)
															{
																if (dZeroTexture <= 0.086)
																{
																	return 1;
																}
																else if (dZeroTexture > 0.086)
																{
																	return 0;
																}
															}
														}
														else if (dAngle > 0.016)
														{
															return 1;
														}
													}
													else if (dSADv > 3.642)
													{
														if (dRsB <= 12.754)
														{
															return 0;
														}
														else if (dRsB > 12.754)
														{
															if (dSADv <= 4.193)
															{
																return 1;
															}
															else if (dSADv > 4.193)
															{
																if (dCount <= 83)
																{
																	if (SADCTPB <= 4.44)
																	{
																		if (dCountDif <= 0.159)
																		{
																			return 1;
																		}
																		else if (dCountDif > 0.159)
																		{
																			return 0;
																		}
																	}
																	else if (SADCTPB > 4.44)
																	{
																		if (dBigTexture <= 2.076)
																		{
																			return 0;
																		}
																		else if (dBigTexture > 2.076)
																		{
																			if (dRsDif <= 10.224)
																			{
																				if (dCountDif <= 0.24)
																				{
																					return 1;
																				}
																				else if (dCountDif > 0.24)
																				{
																					return 0;
																				}
																			}
																			else if (dRsDif > 10.224)
																			{
																				return 0;
																			}
																		}
																	}
																}
																else if (dCount > 83)
																{
																	if (SADCTPB <= 5.008)
																	{
																		if (dCountDif <= 0.672)
																		{
																			if (dSADv <= 4.88)
																			{
																				if (dBigTexture <= 2.007)
																				{
																					if (SADCBPT <= 4.697)
																					{
																						if (dTextureLevel <= -3.8845)
																						{
																							return 0;
																						}
																						else if (dTextureLevel > -3.8845)
																						{
																							return 1;
																						}
																					}
																					else if (SADCBPT > 4.697)
																					{
																						return 0;
																					}
																				}
																				else if (dBigTexture > 2.007)
																				{
																					if (dCount <= 98)
																					{
																						if (SADCTPB <= 4.817)
																						{
																							if (dSADv <= 4.735)
																							{
																								return 0;
																							}
																							else if (dSADv > 4.735)
																							{
																								return 1;
																							}
																						}
																						else if (SADCTPB > 4.817)
																						{
																							return 0;
																						}
																					}
																					else if (dCount > 98)
																					{
																						return 1;
																					}
																				}
																			}
																			else if (dSADv > 4.88)
																			{
																				return 1;
																			}
																		}
																		else if (dCountDif > 0.672)
																		{
																			if (SADCTPB <= 4.911)
																			{
																				return 1;
																			}
																			else if (SADCTPB > 4.911)
																			{
																				if (dAngle <= 0.039)
																				{
																					if (dCountDif <= 4.703)
																					{
																						return 1;
																					}
																					else if (dCountDif > 4.703)
																					{
																						return 0;
																					}
																				}
																				else if (dAngle > 0.039)
																				{
																					if (dSADv <= 5.007)
																					{
																						if (dCount <= 272)
																						{
																							return 0;
																						}
																						else if (dCount > 272)
																						{
																							if (dRsG <= 10.351)
																							{
																								if (dSADv <= 4.98)
																								{
																									return 1;
																								}
																								else if (dSADv > 4.98)
																								{
																									if (dStatCount <= 63)
																									{
																										if (dAngle <= 0.04)
																										{
																											if (dStatDif <= 7.29)
																											{
																												return 0;
																											}
																											else if (dStatDif > 7.29)
																											{
																												return 1;
																											}
																										}
																										else if (dAngle > 0.04)
																										{
																											if (dZeroTexture <= 0.025)
																											{
																												return 1;
																											}
																											else if (dZeroTexture > 0.025)
																											{
																												return 0;
																											}
																										}
																									}
																									else if (dStatCount > 63)
																									{
																										return 1;
																									}
																								}
																							}
																							else if (dRsG > 10.351)
																							{
																								return 1;
																							}
																						}
																					}
																					else if (dSADv > 5.007)
																					{
																						if (dStatCount <= 61)
																						{
																							return 1;
																						}
																						else if (dStatCount > 61)
																						{
																							return 0;
																						}
																					}
																				}
																			}
																		}
																	}
																	else if (SADCTPB > 5.008)
																	{
																		if (dAngle <= 0.042)
																		{
																			if (dTextureLevel <= -3.86)
																			{
																				if (dSADv <= 5.005)
																				{
																					if (dTextureLevel <= -3.88)
																					{
																						if (dCountDif <= 4.276)
																						{
																							return 1;
																						}
																						else if (dCountDif > 4.276)
																						{
																							return 0;
																						}
																					}
																					else if (dTextureLevel > -3.88)
																					{
																						return 1;
																					}
																				}
																				else if (dSADv > 5.005)
																				{
																					if (SADCBPT <= 5.007)
																					{
																						return 0;
																					}
																					else if (SADCBPT > 5.007)
																					{
																						if (dSADv <= 5.018)
																						{
																							if (SADCBPT <= 5.016)
																							{
																								return 1;
																							}
																							else if (SADCBPT > 5.016)
																							{
																								if (dStatDif <= 7.046)
																								{
																									if (dAngle <= 0.038)
																									{
																										return 0;
																									}
																									else if (dAngle > 0.038)
																									{
																										if (dZeroTexture <= 0.025)
																										{
																											return 0;
																										}
																										else if (dZeroTexture > 0.025)
																										{
																											return 1;
																										}
																									}
																								}
																								else if (dStatDif > 7.046)
																								{
																									return 0;
																								}
																							}
																						}
																						else if (dSADv > 5.018)
																						{
																							return 1;
																						}
																					}
																				}
																			}
																			else if (dTextureLevel > -3.86)
																			{
																				return 0;
																			}
																		}
																		else if (dAngle > 0.042)
																		{
																			return 0;
																		}
																	}
																}
															}
														}
													}
												}
												else if (dRsB > 16.209)
												{
													if (SADCTPB <= 4.698)
													{
														return 1;
													}
													else if (SADCTPB > 4.698)
													{
														return 0;
													}
												}
											}
										}
									}
									else if (SADCBPT > 5.08)
									{
										if (dTextureLevel <= -3.934)
										{
											if (dCountDif <= 0.632)
											{
												if (dRsB <= 17.148)
												{
													return 0;
												}
												else if (dRsB > 17.148)
												{
													if (dBigTexture <= 1.265)
													{
														return 0;
													}
													else if (dBigTexture > 1.265)
													{
														return 1;
													}
												}
											}
											else if (dCountDif > 0.632)
											{
												if (dCountDif <= 0.784)
												{
													return 0;
												}
												else if (dCountDif > 0.784)
												{
													return 1;
												}
											}
										}
										else if (dTextureLevel > -3.934)
										{
											if (dRsT <= 14.794)
											{
												return 0;
											}
											else if (dRsT > 14.794)
											{
												if (dRsDif <= 10.951)
												{
													return 1;
												}
												else if (dRsDif > 10.951)
												{
													return 0;
												}
											}
										}
									}
								}
								else if (dSADv > 5.021)
								{
									if (dRsT <= 15.851)
									{
										if (dSADv <= 5.027)
										{
											if (SADCBPT <= 5.032)
											{
												return 1;
											}
											else if (SADCBPT > 5.032)
											{
												if (dRsDif <= 8.432)
												{
													return 0;
												}
												else if (dRsDif > 8.432)
												{
													return 1;
												}
											}
										}
										else if (dSADv > 5.027)
										{
											return 1;
										}
									}
									else if (dRsT > 15.851)
									{
										if (dZeroTexture <= 0.73)
										{
											return 0;
										}
										else if (dZeroTexture > 0.73)
										{
											return 1;
										}
									}
								}
							}
							else if (dTextureLevel > -3.802)
							{
								if (dRsDif <= 3.333)
								{
									if (dCountDif <= 0.128)
									{
										return 0;
									}
									else if (dCountDif > 0.128)
									{
										return 1;
									}
								}
								else if (dRsDif > 3.333)
								{
									if (dCountDif <= 0.06)
									{
										return 1;
									}
									else if (dCountDif > 0.06)
									{
										if (dStatDif <= 0.039)
										{
											if (dTextureLevel <= -3.121)
											{
												return 1;
											}
											else if (dTextureLevel > -3.121)
											{
												return 0;
											}
										}
										else if (dStatDif > 0.039)
										{
											if (dCount <= 111)
											{
												if (dRsB <= 11.991)
												{
													return 0;
												}
												else if (dRsB > 11.991)
												{
													if (dCountDif <= 0.653)
													{
														if (dZeroTexture <= 0.163)
														{
															return 1;
														}
														else if (dZeroTexture > 0.163)
														{
															return 0;
														}
													}
													else if (dCountDif > 0.653)
													{
														return 1;
													}
												}
											}
											else if (dCount > 111)
											{
												if (dStatCount <= 37)
												{
													if (dRsDif <= 4.389)
													{
														return 1;
													}
													else if (dRsDif > 4.389)
													{
														if (dRsG <= 7.839)
														{
															if (dCountDif <= 0.096)
															{
																return 1;
															}
															else if (dCountDif > 0.096)
															{
																return 0;
															}
														}
														else if (dRsG > 7.839)
														{
															return 1;
														}
													}
												}
												else if (dStatCount > 37)
												{
													return 1;
												}
											}
										}
									}
								}
							}
						}
						else if (dRsB > 18.334)
						{
							if (dSADv <= 4.46)
							{
								if (dSADv <= 4.402)
								{
									return 1;
								}
								else if (dSADv > 4.402)
								{
									return 0;
								}
							}
							else if (dSADv > 4.46)
							{
								return 0;
							}
						}
					}
					else if (dCount > 529)
					{
						if (dStatDif <= 0.244)
						{
							if (dRsDif <= 3.994)
							{
								return 1;
							}
							else if (dRsDif > 3.994)
							{
								if (dBigTexture <= 0.239)
								{
									return 0;
								}
								else if (dBigTexture > 0.239)
								{
									if (dRsT <= 14.866)
									{
										return 1;
									}
									else if (dRsT > 14.866)
									{
										return 0;
									}
								}
							}
						}
						else if (dStatDif > 0.244)
						{
							if (dRsT <= 19.134)
							{
								if (SADCBPT <= 7.71)
								{
									if (dCountDif <= 0.878)
									{
										return 1;
									}
									else if (dCountDif > 0.878)
									{
										if (dStatCount <= 163)
										{
											return 0;
										}
										else if (dStatCount > 163)
										{
											return 1;
										}
									}
								}
								else if (SADCBPT > 7.71)
								{
									if (dSADv <= 3.779)
									{
										return 0;
									}
									else if (dSADv > 3.779)
									{
										return 1;
									}
								}
							}
							else if (dRsT > 19.134)
							{
								if (dStatCount <= 429)
								{
									return 0;
								}
								else if (dStatCount > 429)
								{
									return 1;
								}
							}
						}
					}
				}
			}
			else if (dZeroTexture > 2.884)
			{
				if (dDynDif <= 3.51)
				{
					if (dZeroTexture <= 3.402)
					{
						if (dStatDif <= 0.861)
						{
							if (dAngle <= 0)
							{
								return 0;
							}
							else if (dAngle > 0)
							{
								if (dSADv <= 0.917)
								{
									return 1;
								}
								else if (dSADv > 0.917)
								{
									return 0;
								}
							}
						}
						else if (dStatDif > 0.861)
						{
							return 1;
						}
					}
					else if (dZeroTexture > 3.402)
					{
						if (SADCTPB <= 0.477)
						{
							if (dDynDif <= 0.137)
							{
								return 0;
							}
							else if (dDynDif > 0.137)
							{
								return 1;
							}
						}
						else if (SADCTPB > 0.477)
						{
							if (dStatDif <= 0.032)
							{
								return 0;
							}
							else if (dStatDif > 0.032)
							{
								if (SADCBPT <= 1.399)
								{
									if (dStatCount <= 70)
									{
										if (dTextureLevel <= -4.146)
										{
											if (SADCBPT <= 1.246)
											{
												return 1;
											}
											else if (SADCBPT > 1.246)
											{
												if (dRsG <= 5.738)
												{
													return 0;
												}
												else if (dRsG > 5.738)
												{
													return 1;
												}
											}
										}
										else if (dTextureLevel > -4.146)
										{
											return 0;
										}
									}
									else if (dStatCount > 70)
									{
										return 1;
									}
								}
								else if (SADCBPT > 1.399)
								{
									if (dAngle <= 0.001)
									{
										return 0;
									}
									else if (dAngle > 0.001)
									{
										if (dCount <= 293)
										{
											return 0;
										}
										else if (dCount > 293)
										{
											if (dRsDif <= 4.759)
											{
												return 1;
											}
											else if (dRsDif > 4.759)
											{
												if (dCountDif <= 0.13)
												{
													if (dCount <= 666)
													{
														if (dCountDif <= 0.094)
														{
															if (dCountDif <= 0.039)
															{
																if (SADCBPT <= 8.012)
																{
																	return 1;
																}
																else if (SADCBPT > 8.012)
																{
																	return 0;
																}
															}
															else if (dCountDif > 0.039)
															{
																return 0;
															}
														}
														else if (dCountDif > 0.094)
														{
															if (dSADv <= 1.725)
															{
																return 0;
															}
															else if (dSADv > 1.725)
															{
																return 1;
															}
														}
													}
													else if (dCount > 666)
													{
														return 1;
													}
												}
												else if (dCountDif > 0.13)
												{
													if (dAngle <= 0.002)
													{
														if (dRsDif <= 7.909)
														{
															if (dSADv <= 2.09)
															{
																if (dRsDif <= 5.03)
																{
																	return 0;
																}
																else if (dRsDif > 5.03)
																{
																	return 1;
																}
															}
															else if (dSADv > 2.09)
															{
																return 0;
															}
														}
														else if (dRsDif > 7.909)
														{
															return 1;
														}
													}
													else if (dAngle > 0.002)
													{
														if (SADCBPT <= 4.366)
														{
															return 0;
														}
														else if (SADCBPT > 4.366)
														{
															return 1;
														}
													}
												}
											}
										}
									}
								}
							}
						}
					}
				}
				else if (dDynDif > 3.51)
				{
					if (SADCBPT <= 13.099)
					{
						if (SADCTPB <= 3.178)
						{
							if (SADCBPT <= 5.635)
							{
								if (dRsDif <= 5.893)
								{
									return 1;
								}
								else if (dRsDif > 5.893)
								{
									if (dSADv <= 1.818)
									{
										return 0;
									}
									else if (dSADv > 1.818)
									{
										if (dDynDif <= 8.399)
										{
											if (dRsDif <= 7.931)
											{
												if (dCount <= 1114)
												{
													return 1;
												}
												else if (dCount > 1114)
												{
													if (dSADv <= 2.621)
													{
														if (dSADv <= 2.162)
														{
															return 1;
														}
														else if (dSADv > 2.162)
														{
															return 0;
														}
													}
													else if (dSADv > 2.621)
													{
														return 1;
													}
												}
											}
											else if (dRsDif > 7.931)
											{
												return 0;
											}
										}
										else if (dDynDif > 8.399)
										{
											return 1;
										}
									}
								}
							}
							else if (SADCBPT > 5.635)
							{
								if (dStatCount <= 638)
								{
									return 0;
								}
								else if (dStatCount > 638)
								{
									return 1;
								}
							}
						}
						else if (SADCTPB > 3.178)
						{
							if (dStatCount <= 625)
							{
								return 0;
							}
							else if (dStatCount > 625)
							{
								if (dRsDif <= 7.628)
								{
									return 1;
								}
								else if (dRsDif > 7.628)
								{
									return 0;
								}
							}
						}
					}
					else if (SADCBPT > 13.099)
					{
						return 0;
					}
				}
			}
		}
		else if (SADCTPB > 5.02)
		{
			if (dDynDif <= 11.39)
			{
				if (dRsDif <= 15.974)
				{
					if (SADCTPB <= 5.166)
					{
						if (dStatCount <= 355)
						{
							if (dZeroTexture <= 0.187)
							{
								if (SADCBPT <= 5.087)
								{
									return 0;
								}
								else if (SADCBPT > 5.087)
								{
									if (dZeroTexture <= 0.025)
									{
										return 0;
									}
									else if (dZeroTexture > 0.025)
									{
										return 1;
									}
								}
							}
							else if (dZeroTexture > 0.187)
							{
								return 0;
							}
						}
						else if (dStatCount > 355)
						{
							if (dRsG <= 12.423)
							{
								return 1;
							}
							else if (dRsG > 12.423)
							{
								return 0;
							}
						}
					}
					else if (SADCTPB > 5.166)
					{
						if (dCountDif <= 4.155)
						{
							if (dStatCount <= 24)
							{
								if (dCount <= 71)
								{
									return 0;
								}
								else if (dCount > 71)
								{
									return 1;
								}
							}
							else if (dStatCount > 24)
							{
								if (dRsB <= 8.793)
								{
									if (dZeroTexture <= 0.011)
									{
										return 0;
									}
									else if (dZeroTexture > 0.011)
									{
										if (dZeroTexture <= 0.804)
										{
											return 1;
										}
										else if (dZeroTexture > 0.804)
										{
											if (dTextureLevel <= -3.127)
											{
												return 0;
											}
											else if (dTextureLevel > -3.127)
											{
												if (dSADv <= 1.981)
												{
													return 1;
												}
												else if (dSADv > 1.981)
												{
													return 0;
												}
											}
										}
									}
								}
								else if (dRsB > 8.793)
								{
									if (dBigTexture <= 0.291)
									{
										if (dStatCount <= 174)
										{
											return 0;
										}
										else if (dStatCount > 174)
										{
											return 1;
										}
									}
									else if (dBigTexture > 0.291)
									{
										if (dStatCount <= 65)
										{
											if (dStatCount <= 43)
											{
												return 0;
											}
											else if (dStatCount > 43)
											{
												if (dZeroTexture <= 0.023)
												{
													return 1;
												}
												else if (dZeroTexture > 0.023)
												{
													return 0;
												}
											}
										}
										else if (dStatCount > 65)
										{
											if (dAngle <= 0.017)
											{
												if (SADCBPT <= 5.177)
												{
													if (dStatCount <= 302)
													{
														return 0;
													}
													else if (dStatCount > 302)
													{
														return 1;
													}
												}
												else if (SADCBPT > 5.177)
												{
													return 0;
												}
											}
											else if (dAngle > 0.017)
											{
												return 0;
											}
										}
									}
								}
							}
						}
						else if (dCountDif > 4.155)
						{
							if (dRsG <= 12.345)
							{
								if (dSADv <= 6.963)
								{
									return 0;
								}
								else if (dSADv > 6.963)
								{
									return 1;
								}
							}
							else if (dRsG > 12.345)
							{
								return 1;
							}
						}
					}
				}
				else if (dRsDif > 15.974)
				{
					if (SADCTPB <= 7.138)
					{
						if (dCount <= 121)
						{
							if (SADCTPB <= 6.514)
							{
								if (dCountDif <= 0.716)
								{
									if (dStatDif <= 1.342)
									{
										return 1;
									}
									else if (dStatDif > 1.342)
									{
										return 0;
									}
								}
								else if (dCountDif > 0.716)
								{
									return 1;
								}
							}
							else if (SADCTPB > 6.514)
							{
								if (dSADv <= 7.138)
								{
									return 0;
								}
								else if (dSADv > 7.138)
								{
									return 1;
								}
							}
						}
						else if (dCount > 121)
						{
							if (dTextureLevel <= -9.313)
							{
								if (dSADv <= 7.162)
								{
									return 0;
								}
								else if (dSADv > 7.162)
								{
									return 1;
								}
							}
							else if (dTextureLevel > -9.313)
							{
								return 1;
							}
						}
					}
					else if (SADCTPB > 7.138)
					{
						return 0;
					}
				}
			}
			else if (dDynDif > 11.39)
			{
				if (dDynDif <= 219.819)
				{
					if (dBigTexture <= 6.441)
					{
						if (SADCTPB <= 10.377)
						{
							if (dSADv <= 10.373)
							{
								if (dStatCount <= 730)
								{
									if (SADCBPT <= 11.311)
									{
										if (dCount <= 113)
										{
											if (dSADv <= 5.667)
											{
												if (dStatCount <= 100)
												{
													return 0;
												}
												else if (dStatCount > 100)
												{
													if (SADCTPB <= 5.238)
													{
														if (dStatCount <= 109)
														{
															if (dSADv <= 5.138)
															{
																return 0;
															}
															else if (dSADv > 5.138)
															{
																return 1;
															}
														}
														else if (dStatCount > 109)
														{
															return 1;
														}
													}
													else if (SADCTPB > 5.238)
													{
														return 0;
													}
												}
											}
											else if (dSADv > 5.667)
											{
												if (dZeroTexture <= 0.054)
												{
													if (dTextureLevel <= -7.9985)
													{
														if (dBigTexture <= 6.126)
														{
															return 0;
														}
														else if (dBigTexture > 6.126)
														{
															if (dSADv <= 9.38)
															{
																if (dRsG <= 19.182)
																{
																	return 0;
																}
																else if (dRsG > 19.182)
																{
																	return 1;
																}
															}
															else if (dSADv > 9.38)
															{
																if (dSADv <= 10.047)
																{
																	return 0;
																}
																else if (dSADv > 10.047)
																{
																	if (dStatCount <= 55)
																	{
																		return 1;
																	}
																	else if (dStatCount > 55)
																	{
																		return 0;
																	}
																}
															}
														}
													}
													else if (dTextureLevel > -7.9985)
													{
														if (dDynDif <= 17.15)
														{
															if (dRsG <= 14.026)
															{
																return 0;
															}
															else if (dRsG > 14.026)
															{
																if (dTextureLevel <= -6.8055)
																{
																	return 0;
																}
																else if (dTextureLevel > -6.8055)
																{
																	return 1;
																}
															}
														}
														else if (dDynDif > 17.15)
														{
															return 1;
														}
													}
												}
												else if (dZeroTexture > 0.054)
												{
													if (dZeroTexture <= 0.3)
													{
														return 1;
													}
													else if (dZeroTexture > 0.3)
													{
														if (dRsT <= 16.644)
														{
															return 1;
														}
														else if (dRsT > 16.644)
														{
															if (dSADv <= 6.539)
															{
																return 0;
															}
															else if (dSADv > 6.539)
															{
																return 1;
															}
														}
													}
												}
											}
										}
										else if (dCount > 113)
										{
											if (dAngle <= 0.036)
											{
												if (dTextureLevel <= -8.323)
												{
													if (dStatDif <= 18.571)
													{
														return 0;
													}
													else if (dStatDif > 18.571)
													{
														if (dTextureLevel <= -8.7705)
														{
															return 0;
														}
														else if (dTextureLevel > -8.7705)
														{
															return 1;
														}
													}
												}
												else if (dTextureLevel > -8.323)
												{
													if (SADCBPT <= 7.743)
													{
														if (dStatCount <= 590)
														{
															if (dSADv <= 5.212)
															{
																return 0;
															}
															else if (dSADv > 5.212)
															{
																if (dRsG <= 13.413)
																{
																	return 1;
																}
																else if (dRsG > 13.413)
																{
																	return 0;
																}
															}
														}
														else if (dStatCount > 590)
														{
															return 1;
														}
													}
													else if (SADCBPT > 7.743)
													{
														return 0;
													}
												}
											}
											else if (dAngle > 0.036)
											{
												if (dCountDif <= 1.338)
												{
													if (dZeroTexture <= 0.976)
													{
														if (dDynDif <= 14.754)
														{
															return 0;
														}
														else if (dDynDif > 14.754)
														{
															if (dRsDif <= 13.733)
															{
																if (dRsG <= 13.276)
																{
																	if (SADCTPB <= 5.559)
																	{
																		return 1;
																	}
																	else if (SADCTPB > 5.559)
																	{
																		return 0;
																	}
																}
																else if (dRsG > 13.276)
																{
																	if (dAngle <= 0.402)
																	{
																		if (dCount <= 278)
																		{
																			return 1;
																		}
																		else if (dCount > 278)
																		{
																			if (dTextureLevel <= -5.419)
																			{
																				return 0;
																			}
																			else if (dTextureLevel > -5.419)
																			{
																				return 1;
																			}
																		}
																	}
																	else if (dAngle > 0.402)
																	{
																		if (dTextureLevel <= -3.973)
																		{
																			return 0;
																		}
																		else if (dTextureLevel > -3.973)
																		{
																			return 1;
																		}
																	}
																}
															}
															else if (dRsDif > 13.733)
															{
																if (dTextureLevel <= -6.18)
																{
																	if (dAngle <= 0.118)
																	{
																		return 0;
																	}
																	else if (dAngle > 0.118)
																	{
																		return 1;
																	}
																}
																else if (dTextureLevel > -6.18)
																{
																	if (dStatDif <= 49.205)
																	{
																		if (dRsDif <= 13.856)
																		{
																			if (dRsG <= 14.221)
																			{
																				return 0;
																			}
																			else if (dRsG > 14.221)
																			{
																				return 1;
																			}
																		}
																		else if (dRsDif > 13.856)
																		{
																			return 0;
																		}
																	}
																	else if (dStatDif > 49.205)
																	{
																		if (dBigTexture <= 2.846)
																		{
																			return 0;
																		}
																		else if (dBigTexture > 2.846)
																		{
																			return 1;
																		}
																	}
																}
															}
														}
													}
													else if (dZeroTexture > 0.976)
													{
														if (SADCTPB <= 8.814)
														{
															return 0;
														}
														else if (SADCTPB > 8.814)
														{
															if (dTextureLevel <= -6.1985)
															{
																return 0;
															}
															else if (dTextureLevel > -6.1985)
															{
																return 1;
															}
														}
													}
												}
												else if (dCountDif > 1.338)
												{
													if (dCountDif <= 2.387)
													{
														if (dStatCount <= 118)
														{
															if (dRsG <= 13.646)
															{
																if (dStatCount <= 86)
																{
																	return 0;
																}
																else if (dStatCount > 86)
																{
																	if (dTextureLevel <= -6.464)
																	{
																		return 0;
																	}
																	else if (dTextureLevel > -6.464)
																	{
																		return 1;
																	}
																}
															}
															else if (dRsG > 13.646)
															{
																if (dTextureLevel <= -6.5045)
																{
																	if (dSADv <= 10.282)
																	{
																		if (dTextureLevel <= -10.398)
																		{
																			return 0;
																		}
																		else if (dTextureLevel > -10.398)
																		{
																			if (dAngle <= 0.111)
																			{
																				if (dSADv <= 6.47)
																				{
																					return 0;
																				}
																				else if (dSADv > 6.47)
																				{
																					if (dBigTexture <= 3.581)
																					{
																						return 1;
																					}
																					else if (dBigTexture > 3.581)
																					{
																						return 0;
																					}
																				}
																			}
																			else if (dAngle > 0.111)
																			{
																				return 1;
																			}
																		}
																	}
																	else if (dSADv > 10.282)
																	{
																		if (dRsDif <= 25.481)
																		{
																			return 1;
																		}
																		else if (dRsDif > 25.481)
																		{
																			return 0;
																		}
																	}
																}
																else if (dTextureLevel > -6.5045)
																{
																	return 1;
																}
															}
														}
														else if (dStatCount > 118)
														{
															if (dTextureLevel <= -10.5135)
															{
																return 0;
															}
															else if (dTextureLevel > -10.5135)
															{
																if (dDynDif <= 35.996)
																{
																	if (dStatCount <= 126)
																	{
																		return 0;
																	}
																	else if (dStatCount > 126)
																	{
																		return 1;
																	}
																}
																else if (dDynDif > 35.996)
																{
																	return 1;
																}
															}
														}
													}
													else if (dCountDif > 2.387)
													{
														if (SADCTPB <= 10.205)
														{
															if (dSADv <= 10.123)
															{
																if (SADCTPB <= 10.017)
																{
																	if (dZeroTexture <= 0.005)
																	{
																		if (dTextureLevel <= -6.347)
																		{
																			if (dCount <= 353)
																			{
																				if (dStatCount <= 84)
																				{
																					if (dCountDif <= 2.529)
																					{
																						if (dCount <= 338)
																						{
																							if (dStatCount <= 83)
																							{
																								return 0;
																							}
																							else if (dStatCount > 83)
																							{
																								if (dRsG <= 13.639)
																								{
																									return 0;
																								}
																								else if (dRsG > 13.639)
																								{
																									return 1;
																								}
																							}
																						}
																						else if (dCount > 338)
																						{
																							if (dStatCount <= 81)
																							{
																								if (dRsT <= 19.642)
																								{
																									return 0;
																								}
																								else if (dRsT > 19.642)
																								{
																									return 1;
																								}
																							}
																							else if (dStatCount > 81)
																							{
																								return 0;
																							}
																						}
																					}
																					else if (dCountDif > 2.529)
																					{
																						if (dCount <= 125)
																						{
																							if (dDynDif <= 41.177)
																							{
																								return 0;
																							}
																							else if (dDynDif > 41.177)
																							{
																								if (dSADv <= 9.368)
																								{
																									return 1;
																								}
																								else if (dSADv > 9.368)
																								{
																									if (dSADv <= 9.944)
																									{
																										if (dTextureLevel <= -8.321)
																										{
																											if (dRsB <= 29.323)
																											{
																												return 0;
																											}
																											else if (dRsB > 29.323)
																											{
																												if (dStatCount <= 58)
																												{
																													return 0;
																												}
																												else if (dStatCount > 58)
																												{
																													return 1;
																												}
																											}
																										}
																										else if (dTextureLevel > -8.321)
																										{
																											if (dRsG <= 19.301)
																											{
																												return 0;
																											}
																											else if (dRsG > 19.301)
																											{
																												return 1;
																											}
																										}
																									}
																									else if (dSADv > 9.944)
																									{
																										if (dRsDif <= 22.046)
																										{
																											return 1;
																										}
																										else if (dRsDif > 22.046)
																										{
																											return 0;
																										}
																									}
																								}
																							}
																						}
																						else if (dCount > 125)
																						{
																							if (dBigTexture <= 6.293)
																							{
																								if (dBigTexture <= 2.501)
																								{
																									if (dTextureLevel <= -6.4525)
																									{
																										return 0;
																									}
																									else if (dTextureLevel > -6.4525)
																									{
																										if (dRsG <= 13.476)
																										{
																											if (dTextureLevel <= -6.3675)
																											{
																												return 0;
																											}
																											else if (dTextureLevel > -6.3675)
																											{
																												if (dRsG <= 13.401)
																												{
																													return 0;
																												}
																												else if (dRsG > 13.401)
																												{
																													return 1;
																												}
																											}
																										}
																										else if (dRsG > 13.476)
																										{
																											if (dSADv <= 6.268)
																											{
																												return 1;
																											}
																											else if (dSADv > 6.268)
																											{
																												if (dSADv <= 6.303)
																												{
																													return 0;
																												}
																												else if (dSADv > 6.303)
																												{
																													if (dRsG <= 13.561)
																													{
																														return 1;
																													}
																													else if (dRsG > 13.561)
																													{
																														return 0;
																													}
																												}
																											}
																										}
																									}
																								}
																								else if (dBigTexture > 2.501)
																								{
																									if (dTextureLevel <= -8.3995)
																									{
																										if (dBigTexture <= 6.096)
																										{
																											if (dSADv <= 9.694)
																											{
																												if (dSADv <= 9.622)
																												{
																													if (dCountDif <= 4.743)
																													{
																														if (SADCBPT <= 9.543)
																														{
																															if (dRsDif <= 22.77)
																															{
																																return 0;
																															}
																															else if (dRsDif > 22.77)
																															{
																																return 1;
																															}
																														}
																														else if (SADCBPT > 9.543)
																														{
																															return 1;
																														}
																													}
																													else if (dCountDif > 4.743)
																													{
																														return 0;
																													}
																												}
																												else if (dSADv > 9.622)
																												{
																													return 0;
																												}
																											}
																											else if (dSADv > 9.694)
																											{
																												return 1;
																											}
																										}
																										else if (dBigTexture > 6.096)
																										{
																											return 0;
																										}
																									}
																									else if (dTextureLevel > -8.3995)
																									{
																										if (dRsG <= 19.442)
																										{
																											if (dTextureLevel <= -6.4685)
																											{
																												if (dBigTexture <= 2.619)
																												{
																													return 0;
																												}
																												else if (dBigTexture > 2.619)
																												{
																													if (dCount <= 131)
																													{
																														if (dStatCount <= 54)
																														{
																															return 0;
																														}
																														else if (dStatCount > 54)
																														{
																															if (dCount <= 130)
																															{
																																if (dRsT <= 27.148)
																																{
																																	return 1;
																																}
																																else if (dRsT > 27.148)
																																{
																																	return 0;
																																}
																															}
																															else if (dCount > 130)
																															{
																																return 0;
																															}
																														}
																													}
																													else if (dCount > 131)
																													{
																														if (dRsDif <= 13.737)
																														{
																															return 0;
																														}
																														else if (dRsDif > 13.737)
																														{
																															if (dAngle <= 0.345)
																															{
																																if (dRsG <= 19.165)
																																{
																																	if (dTextureLevel <= -8.1105)
																																	{
																																		return 0;
																																	}
																																	else if (dTextureLevel > -8.1105)
																																	{
																																		return 1;
																																	}
																																}
																																else if (dRsG > 19.165)
																																{
																																	return 1;
																																}
																															}
																															else if (dAngle > 0.345)
																															{
																																if (dRsT <= 27.09)
																																{
																																	return 1;
																																}
																																else if (dRsT > 27.09)
																																{
																																	return 0;
																																}
																															}
																														}
																													}
																												}
																											}
																											else if (dTextureLevel > -6.4685)
																											{
																												if (dStatCount <= 77)
																												{
																													if (dSADv <= 6.275)
																													{
																														return 0;
																													}
																													else if (dSADv > 6.275)
																													{
																														return 1;
																													}
																												}
																												else if (dStatCount > 77)
																												{
																													return 1;
																												}
																											}
																										}
																										else if (dRsG > 19.442)
																										{
																											return 1;
																										}
																									}
																								}
																							}
																							else if (dBigTexture > 6.293)
																							{
																								if (dSADv <= 9.894)
																								{
																									if (dDynDif <= 42.481)
																									{
																										if (dStatDif <= 37.189)
																										{
																											return 1;
																										}
																										else if (dStatDif > 37.189)
																										{
																											return 0;
																										}
																									}
																									else if (dDynDif > 42.481)
																									{
																										return 1;
																									}
																								}
																								else if (dSADv > 9.894)
																								{
																									if (dSADv <= 9.918)
																									{
																										return 0;
																									}
																									else if (dSADv > 9.918)
																									{
																										if (dCount <= 129)
																										{
																											return 0;
																										}
																										else if (dCount > 129)
																										{
																											if (dRsT <= 27.908)
																											{
																												if (dAngle <= 0.348)
																												{
																													return 1;
																												}
																												else if (dAngle > 0.348)
																												{
																													return 0;
																												}
																											}
																											else if (dRsT > 27.908)
																											{
																												return 0;
																											}
																										}
																									}
																								}
																							}
																						}
																					}
																				}
																				else if (dStatCount > 84)
																				{
																					if (dTextureLevel <= -6.446)
																					{
																						if (dSADv <= 6.425)
																						{
																							if (dBigTexture <= 2.624)
																							{
																								if (dSADv <= 6.154)
																								{
																									return 1;
																								}
																								else if (dSADv > 6.154)
																								{
																									return 0;
																								}
																							}
																							else if (dBigTexture > 2.624)
																							{
																								return 1;
																							}
																						}
																						else if (dSADv > 6.425)
																						{
																							if (dRsT <= 29.629)
																							{
																								return 1;
																							}
																							else if (dRsT > 29.629)
																							{
																								return 0;
																							}
																						}
																					}
																					else if (dTextureLevel > -6.446)
																					{
																						return 1;
																					}
																				}
																			}
																			else if (dCount > 353)
																			{
																				return 1;
																			}
																		}
																		else if (dTextureLevel > -6.347)
																		{
																			if (dCountDif <= 4.296)
																			{
																				return 1;
																			}
																			else if (dCountDif > 4.296)
																			{
																				if (dCountDif <= 4.826)
																				{
																					return 1;
																				}
																				else if (dCountDif > 4.826)
																				{
																					return 0;
																				}
																			}
																		}
																	}
																	else if (dZeroTexture > 0.005)
																	{
																		if (SADCBPT <= 5.043)
																		{
																			return 0;
																		}
																		else if (SADCBPT > 5.043)
																		{
																			if (dCount <= 471)
																			{
																				if (SADCTPB <= 5.057)
																				{
																					if (dSADv <= 5.052)
																					{
																						if (dStatDif <= 7.548)
																						{
																							if (dTextureLevel <= -3.9025)
																							{
																								return 1;
																							}
																							else if (dTextureLevel > -3.9025)
																							{
																								if (dDynDif <= 11.607)
																								{
																									if (SADCBPT <= 5.046)
																									{
																										return 1;
																									}
																									else if (SADCBPT > 5.046)
																									{
																										return 0;
																									}
																								}
																								else if (dDynDif > 11.607)
																								{
																									if (dZeroTexture <= 0.025)
																									{
																										return 1;
																									}
																									else if (dZeroTexture > 0.025)
																									{
																										if (dSADv <= 5.042)
																										{
																											if (dSADv <= 5.027)
																											{
																												if (dStatDif <= 7.33)
																												{
																													return 1;
																												}
																												else if (dStatDif > 7.33)
																												{
																													return 0;
																												}
																											}
																											else if (dSADv > 5.027)
																											{
																												return 0;
																											}
																										}
																										else if (dSADv > 5.042)
																										{
																											return 1;
																										}
																									}
																								}
																							}
																						}
																						else if (dStatDif > 7.548)
																						{
																							return 0;
																						}
																					}
																					else if (dSADv > 5.052)
																					{
																						if (dRsT <= 14.265)
																						{
																							return 1;
																						}
																						else if (dRsT > 14.265)
																						{
																							if (dRsDif <= 8.513)
																							{
																								if (dAngle <= 0.04)
																								{
																									return 1;
																								}
																								else if (dAngle > 0.04)
																								{
																									return 0;
																								}
																							}
																							else if (dRsDif > 8.513)
																							{
																								return 1;
																							}
																						}
																					}
																				}
																				else if (SADCTPB > 5.057)
																				{
																					if (SADCBPT <= 5.236)
																					{
																						if (dSADv <= 5.236)
																						{
																							if (SADCBPT <= 5.195)
																							{
																								if (dSADv <= 5.199)
																								{
																									if (dTextureLevel <= -3.841)
																									{
																										if (SADCTPB <= 5.2)
																										{
																											if (dAngle <= 0.052)
																											{
																												return 0;
																											}
																											else if (dAngle > 0.052)
																											{
																												if (dDynDif <= 15.386)
																												{
																													if (dCount <= 261)
																													{
																														return 1;
																													}
																													else if (dCount > 261)
																													{
																														return 0;
																													}
																												}
																												else if (dDynDif > 15.386)
																												{
																													return 1;
																												}
																											}
																										}
																										else if (SADCTPB > 5.2)
																										{
																											if (SADCTPB <= 5.206)
																											{
																												return 1;
																											}
																											else if (SADCTPB > 5.206)
																											{
																												if (dRsT <= 14.605)
																												{
																													return 1;
																												}
																												else if (dRsT > 14.605)
																												{
																													if (dRsT <= 14.703)
																													{
																														return 0;
																													}
																													else if (dRsT > 14.703)
																													{
																														return 1;
																													}
																												}
																											}
																										}
																									}
																									else if (dTextureLevel > -3.841)
																									{
																										if (dZeroTexture <= 0.025)
																										{
																											return 1;
																										}
																										else if (dZeroTexture > 0.025)
																										{
																											if (dSADv <= 5.192)
																											{
																												if (dRsG <= 10.751)
																												{
																													if (dZeroTexture <= 0.034)
																													{
																														return 0;
																													}
																													else if (dZeroTexture > 0.034)
																													{
																														if (dZeroTexture <= 0.044)
																														{
																															return 1;
																														}
																														else if (dZeroTexture > 0.044)
																														{
																															if (dAngle <= 0.047)
																															{
																																if (dZeroTexture <= 0.064)
																																{
																																	return 1;
																																}
																																else if (dZeroTexture > 0.064)
																																{
																																	return 0;
																																}
																															}
																															else if (dAngle > 0.047)
																															{
																																return 0;
																															}
																														}
																													}
																												}
																												else if (dRsG > 10.751)
																												{
																													if (dStatCount <= 95)
																													{
																														return 1;
																													}
																													else if (dStatCount > 95)
																													{
																														return 0;
																													}
																												}
																											}
																											else if (dSADv > 5.192)
																											{
																												if (SADCTPB <= 5.227)
																												{
																													return 0;
																												}
																												else if (SADCTPB > 5.227)
																												{
																													return 1;
																												}
																											}
																										}
																									}
																								}
																								else if (dSADv > 5.199)
																								{
																									if (dCount <= 301)
																									{
																										if (dSADv <= 5.226)
																										{
																											if (dZeroTexture <= 0.044)
																											{
																												return 0;
																											}
																											else if (dZeroTexture > 0.044)
																											{
																												if (dRsT <= 14.689)
																												{
																													return 1;
																												}
																												else if (dRsT > 14.689)
																												{
																													return 0;
																												}
																											}
																										}
																										else if (dSADv > 5.226)
																										{
																											return 1;
																										}
																									}
																									else if (dCount > 301)
																									{
																										return 0;
																									}
																								}
																							}
																							else if (SADCBPT > 5.195)
																							{
																								if (dStatCount <= 71)
																								{
																									return 1;
																								}
																								else if (dStatCount > 71)
																								{
																									if (dSADv <= 5.195)
																									{
																										if (dTextureLevel <= -3.769)
																										{
																											if (dSADv <= 5.172)
																											{
																												if (dBigTexture <= 1.593)
																												{
																													if (dRsB <= 14.508)
																													{
																														return 1;
																													}
																													else if (dRsB > 14.508)
																													{
																														return 0;
																													}
																												}
																												else if (dBigTexture > 1.593)
																												{
																													if (dStatDif <= 10.88)
																													{
																														if (SADCBPT <= 5.201)
																														{
																															if (dSADv <= 5.16)
																															{
																																return 1;
																															}
																															else if (dSADv > 5.16)
																															{
																																return 0;
																															}
																														}
																														else if (SADCBPT > 5.201)
																														{
																															return 1;
																														}
																													}
																													else if (dStatDif > 10.88)
																													{
																														return 0;
																													}
																												}
																											}
																											else if (dSADv > 5.172)
																											{
																												if (dBigTexture <= 1.623)
																												{
																													return 0;
																												}
																												else if (dBigTexture > 1.623)
																												{
																													if (dDynDif <= 14.139)
																													{
																														return 1;
																													}
																													else if (dDynDif > 14.139)
																													{
																														return 0;
																													}
																												}
																											}
																										}
																										else if (dTextureLevel > -3.769)
																										{
																											return 1;
																										}
																									}
																									else if (dSADv > 5.195)
																									{
																										if (SADCTPB <= 5.177)
																										{
																											return 1;
																										}
																										else if (SADCTPB > 5.177)
																										{
																											if (dStatCount <= 82)
																											{
																												if (dRsDif <= 8.787)
																												{
																													if (SADCBPT <= 5.223)
																													{
																														return 0;
																													}
																													else if (SADCBPT > 5.223)
																													{
																														return 1;
																													}
																												}
																												else if (dRsDif > 8.787)
																												{
																													if (dBigTexture <= 1.631)
																													{
																														if (dZeroTexture <= 0.054)
																														{
																															return 1;
																														}
																														else if (dZeroTexture > 0.054)
																														{
																															if (SADCTPB <= 5.201)
																															{
																																return 0;
																															}
																															else if (SADCTPB > 5.201)
																															{
																																return 1;
																															}
																														}
																													}
																													else if (dBigTexture > 1.631)
																													{
																														if (dStatDif <= 10.041)
																														{
																															return 1;
																														}
																														else if (dStatDif > 10.041)
																														{
																															return 0;
																														}
																													}
																												}
																											}
																											else if (dStatCount > 82)
																											{
																												if (dTextureLevel <= -3.7055)
																												{
																													if (dSADv <= 5.207)
																													{
																														if (SADCBPT <= 5.231)
																														{
																															return 1;
																														}
																														else if (SADCBPT > 5.231)
																														{
																															return 0;
																														}
																													}
																													else if (dSADv > 5.207)
																													{
																														if (SADCBPT <= 5.208)
																														{
																															return 0;
																														}
																														else if (SADCBPT > 5.208)
																														{
																															if (dStatCount <= 84)
																															{
																																if (dRsT <= 14.546)
																																{
																																	return 0;
																																}
																																else if (dRsT > 14.546)
																																{
																																	if (dStatDif <= 10.999)
																																	{
																																		return 0;
																																	}
																																	else if (dStatDif > 10.999)
																																	{
																																		if (dBigTexture <= 1.617)
																																		{
																																			return 1;
																																		}
																																		else if (dBigTexture > 1.617)
																																		{
																																			return 0;
																																		}
																																	}
																																}
																															}
																															else if (dStatCount > 84)
																															{
																																return 1;
																															}
																														}
																													}
																												}
																												else if (dTextureLevel > -3.7055)
																												{
																													return 0;
																												}
																											}
																										}
																									}
																								}
																							}
																						}
																						else if (dSADv > 5.236)
																						{
																							if (dTextureLevel <= -3.8195)
																							{
																								if (dRsDif <= 8.901)
																								{
																									return 0;
																								}
																								else if (dRsDif > 8.901)
																								{
																									if (dBigTexture <= 1.647)
																									{
																										return 1;
																									}
																									else if (dBigTexture > 1.647)
																									{
																										return 0;
																									}
																								}
																							}
																							else if (dTextureLevel > -3.8195)
																							{
																								return 0;
																							}
																						}
																					}
																					else if (SADCBPT > 5.236)
																					{
																						if (dStatDif <= 10.597)
																						{
																							return 1;
																						}
																						else if (dStatDif > 10.597)
																						{
																							if (dRsG <= 10.759)
																							{
																								if (dStatDif <= 10.737)
																								{
																									return 0;
																								}
																								else if (dStatDif > 10.737)
																								{
																									if (dCountDif <= 5.383)
																									{
																										return 1;
																									}
																									else if (dCountDif > 5.383)
																									{
																										if (dCount <= 356)
																										{
																											return 1;
																										}
																										else if (dCount > 356)
																										{
																											return 0;
																										}
																									}
																								}
																							}
																							else if (dRsG > 10.759)
																							{
																								if (SADCTPB <= 8.808)
																								{
																									if (dSADv <= 8.807)
																									{
																										if (SADCBPT <= 8.803)
																										{
																											if (dTextureLevel <= -6.4155)
																											{
																												if (dSADv <= 8.593)
																												{
																													if (SADCBPT <= 8.586)
																													{
																														if (dSADv <= 8.549)
																														{
																															if (dSADv <= 8.111)
																															{
																																return 0;
																															}
																															else if (dSADv > 8.111)
																															{
																																if (SADCTPB <= 8.111)
																																{
																																	if (SADCBPT <= 8.194)
																																	{
																																		return 1;
																																	}
																																	else if (SADCBPT > 8.194)
																																	{
																																		return 0;
																																	}
																																}
																																else if (SADCTPB > 8.111)
																																{
																																	if (SADCBPT <= 8.547)
																																	{
																																		if (dSADv <= 8.173)
																																		{
																																			if (SADCBPT <= 8.156)
																																			{
																																				if (dSADv <= 8.136)
																																				{
																																					return 0;
																																				}
																																				else if (dSADv > 8.136)
																																				{
																																					return 1;
																																				}
																																			}
																																			else if (SADCBPT > 8.156)
																																			{
																																				return 0;
																																			}
																																		}
																																		else if (dSADv > 8.173)
																																		{
																																			if (dRsB <= 23.717)
																																			{
																																				return 1;
																																			}
																																			else if (dRsB > 23.717)
																																			{
																																				if (dStatCount <= 267)
																																				{
																																					if (dCountDif <= 2.885)
																																					{
																																						return 1;
																																					}
																																					else if (dCountDif > 2.885)
																																					{
																																						if (dCount <= 369)
																																						{
																																							return 0;
																																						}
																																						else if (dCount > 369)
																																						{
																																							if (dSADv <= 8.494)
																																							{
																																								return 0;
																																							}
																																							else if (dSADv > 8.494)
																																							{
																																								return 1;
																																							}
																																						}
																																					}
																																				}
																																				else if (dStatCount > 267)
																																				{
																																					return 1;
																																				}
																																			}
																																		}
																																	}
																																	else if (SADCBPT > 8.547)
																																	{
																																		return 0;
																																	}
																																}
																															}
																														}
																														else if (dSADv > 8.549)
																														{
																															if (dCount <= 388)
																															{
																																return 1;
																															}
																															else if (dCount > 388)
																															{
																																return 0;
																															}
																														}
																													}
																													else if (SADCBPT > 8.586)
																													{
																														return 0;
																													}
																												}
																												else if (dSADv > 8.593)
																												{
																													return 1;
																												}
																											}
																											else if (dTextureLevel > -6.4155)
																											{
																												return 0;
																											}
																										}
																										else if (SADCBPT > 8.803)
																										{
																											if (SADCTPB <= 8.71)
																											{
																												return 1;
																											}
																											else if (SADCTPB > 8.71)
																											{
																												return 0;
																											}
																										}
																									}
																									else if (dSADv > 8.807)
																									{
																										if (SADCBPT <= 8.822)
																										{
																											return 1;
																										}
																										else if (SADCBPT > 8.822)
																										{
																											if (dSADv <= 8.826)
																											{
																												if (dRsT <= 24.798)
																												{
																													if (dTextureLevel <= -5.9075)
																													{
																														return 0;
																													}
																													else if (dTextureLevel > -5.9075)
																													{
																														return 1;
																													}
																												}
																												else if (dRsT > 24.798)
																												{
																													return 1;
																												}
																											}
																											else if (dSADv > 8.826)
																											{
																												return 1;
																											}
																										}
																									}
																								}
																								else if (SADCTPB > 8.808)
																								{
																									if (dSADv <= 8.846)
																									{
																										if (SADCBPT <= 8.847)
																										{
																											if (dSADv <= 8.819)
																											{
																												if (dBigTexture <= 3.969)
																												{
																													if (dRsG <= 18.585)
																													{
																														return 0;
																													}
																													else if (dRsG > 18.585)
																													{
																														if (dCount <= 416)
																														{
																															return 1;
																														}
																														else if (dCount > 416)
																														{
																															return 0;
																														}
																													}
																												}
																												else if (dBigTexture > 3.969)
																												{
																													if (SADCBPT <= 8.823)
																													{
																														return 1;
																													}
																													else if (SADCBPT > 8.823)
																													{
																														return 0;
																													}
																												}
																											}
																											else if (dSADv > 8.819)
																											{
																												if (SADCBPT <= 8.82)
																												{
																													if (dRsDif <= 13.763)
																													{
																														return 1;
																													}
																													else if (dRsDif > 13.763)
																													{
																														return 0;
																													}
																												}
																												else if (SADCBPT > 8.82)
																												{
																													if (SADCTPB <= 8.822)
																													{
																														return 1;
																													}
																													else if (SADCTPB > 8.822)
																													{
																														if (dSADv <= 8.822)
																														{
																															if (SADCTPB <= 8.858)
																															{
																																return 1;
																															}
																															else if (SADCTPB > 8.858)
																															{
																																return 0;
																															}
																														}
																														else if (dSADv > 8.822)
																														{
																															if (dAngle <= 0.135)
																															{
																																if (dZeroTexture <= 2.005)
																																{
																																	return 0;
																																}
																																else if (dZeroTexture > 2.005)
																																{
																																	if (dRsT <= 24.658)
																																	{
																																		return 0;
																																	}
																																	else if (dRsT > 24.658)
																																	{
																																		return 1;
																																	}
																																}
																															}
																															else if (dAngle > 0.135)
																															{
																																return 1;
																															}
																														}
																													}
																												}
																											}
																										}
																										else if (SADCBPT > 8.847)
																										{
																											if (dRsB <= 24.377)
																											{
																												if (dStatDif <= 52.957)
																												{
																													return 1;
																												}
																												else if (dStatDif > 52.957)
																												{
																													return 0;
																												}
																											}
																											else if (dRsB > 24.377)
																											{
																												return 0;
																											}
																										}
																									}
																									else if (dSADv > 8.846)
																									{
																										if (SADCTPB <= 8.837)
																										{
																											if (SADCBPT <= 8.852)
																											{
																												return 1;
																											}
																											else if (SADCBPT > 8.852)
																											{
																												if (dRsDif <= 13.851)
																												{
																													return 0;
																												}
																												else if (dRsDif > 13.851)
																												{
																													if (dRsB <= 24.526)
																													{
																														return 1;
																													}
																													else if (dRsB > 24.526)
																													{
																														if (dRsG <= 18.562)
																														{
																															return 0;
																														}
																														else if (dRsG > 18.562)
																														{
																															if (dRsB <= 24.542)
																															{
																																return 0;
																															}
																															else if (dRsB > 24.542)
																															{
																																return 1;
																															}
																														}
																													}
																												}
																											}
																										}
																										else if (SADCTPB > 8.837)
																										{
																											if (dSADv <= 8.887)
																											{
																												return 0;
																											}
																											else if (dSADv > 8.887)
																											{
																												if (SADCTPB <= 8.889)
																												{
																													if (dRsG <= 18.798)
																													{
																														if (SADCBPT <= 8.917)
																														{
																															return 1;
																														}
																														else if (SADCBPT > 8.917)
																														{
																															if (dSADv <= 8.901)
																															{
																																return 0;
																															}
																															else if (dSADv > 8.901)
																															{
																																return 1;
																															}
																														}
																													}
																													else if (dRsG > 18.798)
																													{
																														return 0;
																													}
																												}
																												else if (SADCTPB > 8.889)
																												{
																													return 0;
																												}
																											}
																										}
																									}
																								}
																							}
																						}
																					}
																				}
																			}
																			else if (dCount > 471)
																			{
																				if (SADCTPB <= 9.282)
																				{
																					return 0;
																				}
																				else if (SADCTPB > 9.282)
																				{
																					if (dBigTexture <= 4.429)
																					{
																						if (dCount <= 472)
																						{
																							return 0;
																						}
																						else if (dCount > 472)
																						{
																							return 1;
																						}
																					}
																					else if (dBigTexture > 4.429)
																					{
																						return 0;
																					}
																				}
																			}
																		}
																	}
																}
																else if (SADCTPB > 10.017)
																{
																	if (dBigTexture <= 6.372)
																	{
																		if (dSADv <= 10.053)
																		{
																			return 0;
																		}
																		else if (dSADv > 10.053)
																		{
																			if (SADCTPB <= 10.051)
																			{
																				return 1;
																			}
																			else if (SADCTPB > 10.051)
																			{
																				return 0;
																			}
																		}
																	}
																	else if (dBigTexture > 6.372)
																	{
																		if (dCount <= 135)
																		{
																			if (dAngle <= 0.33)
																			{
																				if (dAngle <= 0.319)
																				{
																					return 0;
																				}
																				else if (dAngle > 0.319)
																				{
																					return 1;
																				}
																			}
																			else if (dAngle > 0.33)
																			{
																				return 0;
																			}
																		}
																		else if (dCount > 135)
																		{
																			return 1;
																		}
																	}
																}
															}
															else if (dSADv > 10.123)
															{
																if (SADCTPB <= 10.156)
																{
																	return 1;
																}
																else if (SADCTPB > 10.156)
																{
																	if (dStatCount <= 111)
																	{
																		return 0;
																	}
																	else if (dStatCount > 111)
																	{
																		return 1;
																	}
																}
															}
														}
														else if (SADCTPB > 10.205)
														{
															return 0;
														}
													}
												}
											}
										}
									}
									else if (SADCBPT > 11.311)
									{
										if (dAngle <= 0.165)
										{
											return 0;
										}
										else if (dAngle > 0.165)
										{
											if (dRsG <= 16.545)
											{
												return 1;
											}
											else if (dRsG > 16.545)
											{
												return 0;
											}
										}
									}
								}
								else if (dStatCount > 730)
								{
									if (dTextureLevel <= -8.7485)
									{
										if (dAngle <= 0.034)
										{
											return 0;
										}
										else if (dAngle > 0.034)
										{
											return 1;
										}
									}
									else if (dTextureLevel > -8.7485)
									{
										return 1;
									}
								}
							}
							else if (dSADv > 10.373)
							{
								return 1;
							}
						}
						else if (SADCTPB > 10.377)
						{
							if (dBigTexture <= 1.107)
							{
								if (dAngle <= 0.047)
								{
									if (dAngle <= 0.01)
									{
										return 1;
									}
									else if (dAngle > 0.01)
									{
										return 0;
									}
								}
								else if (dAngle > 0.047)
								{
									return 1;
								}
							}
							else if (dBigTexture > 1.107)
							{
								if (dDynDif <= 87.363)
								{
									if (dStatCount <= 1000)
									{
										if (dCountDif <= 3.668)
										{
											return 0;
										}
										else if (dCountDif > 3.668)
										{
											if (dStatCount <= 236)
											{
												return 0;
											}
											else if (dStatCount > 236)
											{
												if (dStatDif <= 82.749)
												{
													if (dStatCount <= 254)
													{
														return 1;
													}
													else if (dStatCount > 254)
													{
														return 0;
													}
												}
												else if (dStatDif > 82.749)
												{
													return 0;
												}
											}
										}
									}
									else if (dStatCount > 1000)
									{
										if (SADCBPT <= 9.396)
										{
											return 1;
										}
										else if (SADCBPT > 9.396)
										{
											if (SADCBPT <= 50.805)
											{
												return 0;
											}
											else if (SADCBPT > 50.805)
											{
												return 1;
											}
										}
									}
								}
								else if (dDynDif > 87.363)
								{
									if (dStatCount <= 244)
									{
										return 0;
									}
									else if (dStatCount > 244)
									{
										if (dTextureLevel <= -10.139)
										{
											if (dCount <= 500)
											{
												return 0;
											}
											else if (dCount > 500)
											{
												return 1;
											}
										}
										else if (dTextureLevel > -10.139)
										{
											return 1;
										}
									}
								}
							}
						}
					}
					else if (dBigTexture > 6.441)
					{
						if (dTextureLevel <= -8.5135)
						{
							if (dRsG <= 34.197)
							{
								if (dRsDif <= 20.533)
								{
									return 1;
								}
								else if (dRsDif > 20.533)
								{
									if (dCount <= 104)
									{
										return 0;
									}
									else if (dCount > 104)
									{
										if (dBigTexture <= 8.927)
										{
											if (dStatCount <= 370)
											{
												if (dCount <= 458)
												{
													if (dTextureLevel <= -8.7665)
													{
														if (dStatCount <= 334)
														{
															return 0;
														}
														else if (dStatCount > 334)
														{
															if (dRsT <= 45.412)
															{
																return 1;
															}
															else if (dRsT > 45.412)
															{
																return 0;
															}
														}
													}
													else if (dTextureLevel > -8.7665)
													{
														if (dRsG <= 19.736)
														{
															if (SADCTPB <= 9.936)
															{
																if (dDynDif <= 42.887)
																{
																	if (dAngle <= 0.313)
																	{
																		if (dRsG <= 19.385)
																		{
																			return 0;
																		}
																		else if (dRsG > 19.385)
																		{
																			return 1;
																		}
																	}
																	else if (dAngle > 0.313)
																	{
																		return 0;
																	}
																}
																else if (dDynDif > 42.887)
																{
																	return 1;
																}
															}
															else if (SADCTPB > 9.936)
															{
																return 0;
															}
														}
														else if (dRsG > 19.736)
														{
															return 1;
														}
													}
												}
												else if (dCount > 458)
												{
													if (dTextureLevel <= -12.6445)
													{
														return 0;
													}
													else if (dTextureLevel > -12.6445)
													{
														if (dStatCount <= 239)
														{
															return 0;
														}
														else if (dStatCount > 239)
														{
															return 1;
														}
													}
												}
											}
											else if (dStatCount > 370)
											{
												if (dAngle <= 0.377)
												{
													return 1;
												}
												else if (dAngle > 0.377)
												{
													return 0;
												}
											}
										}
										else if (dBigTexture > 8.927)
										{
											return 0;
										}
									}
								}
							}
							else if (dRsG > 34.197)
							{
								return 1;
							}
						}
						else if (dTextureLevel > -8.5135)
						{
							if (SADCBPT <= 23.771)
							{
								return 1;
							}
							else if (SADCBPT > 23.771)
							{
								return 0;
							}
						}
					}
				}
				else if (dDynDif > 219.819)
				{
					if (dTextureLevel <= -10.033)
					{
						if (SADCTPB <= 23.486)
						{
							return 1;
						}
						else if (SADCTPB > 23.486)
						{
							return 0;
						}
					}
					else if (dTextureLevel > -10.033)
					{
						if (dStatCount <= 1183)
						{
							if (dTextureLevel <= -8.062)
							{
								return 0;
							}
							else if (dTextureLevel > -8.062)
							{
								if (SADCBPT <= 25.336)
								{
									if (dRsDif <= 29.926)
									{
										return 1;
									}
									else if (dRsDif > 29.926)
									{
										return 0;
									}
								}
								else if (SADCBPT > 25.336)
								{
									return 0;
								}
							}
						}
						else if (dStatCount > 1183)
						{
							if (dRsDif <= 36.944)
							{
								if (dSADv <= 34.003)
								{
									return 0;
								}
								else if (dSADv > 34.003)
								{
									return 1;
								}
							}
							else if (dRsDif > 36.944)
							{
								if (dStatCount <= 1282)
								{
									return 0;
								}
								else if (dStatCount > 1282)
								{
									return 1;
								}
							}
						}
					}
				}
			}
		}
	}

	return 3;
}