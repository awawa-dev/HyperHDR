#pragma once

///
/// The xml to fill with data
/// %1 base url                   http://192.168.1.36:8090/
/// %2 friendly name              HyperHDR (192.168.1.36)
/// %3 modelNumber                17.0.0
/// %4 serialNumber / UDN (H ID)  Fjsa723dD0....
///
/// def URN urn:schemas-upnp-org:device:Basic:1

static constexpr const char* SSDP_DESCRIPTION =	"<?xml version=\"1.0\"?>"
										"<root xmlns=\"urn:schemas-upnp-org:device-1-0\">"
											"<specVersion>"
												"<major>1</major>"
												"<minor>0</minor>"
											"</specVersion>"
											"<URLBase>%1</URLBase>"
											"<device>"
												"<deviceType>urn:schemas-upnp-org:device:Basic:1</deviceType>"
												"<friendlyName>%2</friendlyName>"
												"<manufacturer>HyperHDR Open Source Ambient Lighting</manufacturer>"
												"<manufacturerURL>http://www.hyperhdr.eu/</manufacturerURL>"
												"<modelDescription>HyperHDR Open Source Ambient Light</modelDescription>"
												"<modelName>HyperHDR</modelName>"
												"<modelNumber>%3</modelNumber>"
												"<modelURL>https://github.com/awawa-dev/HyperHDR</modelURL>"
												"<serialNumber>%4</serialNumber>"
												"<UDN>uuid:%4</UDN>"
												"<ports>"
													"<jsonServer>%5</jsonServer>"
													"<sslServer>%6</sslServer>"
													"<protoBuffer>%7</protoBuffer>"
													"<flatBuffer>%8</flatBuffer>"
												"</ports>"
												"<presentationURL>index.html</presentationURL>"
												"<iconList>"
													"<icon>"
														"<mimetype>image/png</mimetype>"
														"<height>100</height>"
														"<width>100</width>"
														"<depth>32</depth>"
														"<url>img/hyperhdr/ssdp_icon.png</url>"
													"</icon>"
												"</iconList>"
											"</device>"
										"</root>";
