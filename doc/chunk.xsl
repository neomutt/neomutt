<?xml version="1.0"?>
<!DOCTYPE xsl:stylesheet [
<!ENTITY css SYSTEM "mutt.css">
]>
<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
  version="1.0">
  <xsl:import href="http://docbook.sourceforge.net/release/xsl/current/xhtml/chunk.xsl"/>
  <xsl:param name="section.autolabel" select="1"/>
  <xsl:param name="use.id.as.filename" select="1"/>
  <xsl:param name="chunk.section.depth" select="0"/>
  <xsl:template name="user.head.content">
    <xsl:element name="style">
      <xsl:attribute name="type">text/css</xsl:attribute>
      &css;
    </xsl:element>
  </xsl:template>
</xsl:stylesheet>
