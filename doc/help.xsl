<?xml version='1.0'?>
<!--
	Convert DocBook documentation to help.txt file used by bitlbee
	(C) 2004 Jelmer Vernooij
-->
<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
	xmlns:exsl="http://exslt.org/common"
	xmlns:samba="http://samba.org/common"
	version="1.1"
	extension-element-prefixes="exsl">

	<xsl:output method="text" encoding="iso-8859-1" standalone="yes"/>
	<xsl:strip-space elements="*"/>

	<xsl:param name="xmlSambaNsUri" select="'http://samba.org/common'"/>

	<xsl:template match="para">
		<xsl:apply-templates/>
	</xsl:template>

	<xsl:template name="subject">
		<xsl:message><xsl:text>Processing: </xsl:text><xsl:value-of select="$id"/></xsl:message>
		<xsl:text>?</xsl:text><xsl:value-of select="$id"/><xsl:text>&#10;</xsl:text>
		
		<xsl:for-each select="para|variablelist|simplelist">
			<xsl:apply-templates select="."/>
		</xsl:for-each>
		<xsl:text>&#10;%&#10;</xsl:text>

		<xsl:for-each select="sect1|sect2">
			<xsl:call-template name="subject">
				<xsl:with-param name="id" select="@id"/>
			</xsl:call-template>
		</xsl:for-each>
	</xsl:template>

	<xsl:template match="preface|chapter|sect1|sect2">
		<xsl:call-template name="subject">
			<xsl:with-param name="id" select="@id"/>
		</xsl:call-template>
	</xsl:template>

	<xsl:template match="emphasis">
		<xsl:text>_b_</xsl:text>
		<xsl:apply-templates/>
		<xsl:text>_b_</xsl:text>
	</xsl:template>

	<xsl:template match="book">
		<xsl:apply-templates/>
	</xsl:template>

	<xsl:template match="variablelist">
		<xsl:for-each select="varlistentry">
			<xsl:text> _b_</xsl:text><xsl:value-of select="term"/><xsl:text>_b_ - </xsl:text><xsl:value-of select="listitem/para"/><xsl:text>&#10;</xsl:text>
		</xsl:for-each>
	</xsl:template>

	<xsl:template match="simplelist">
		<xsl:for-each select="member">
			<xsl:text> - </xsl:text><xsl:apply-templates/><xsl:text>&#10;</xsl:text>
		</xsl:for-each>
	</xsl:template>

</xsl:stylesheet>
