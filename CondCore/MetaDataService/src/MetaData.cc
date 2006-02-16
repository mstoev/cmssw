#include "CondCore/MetaDataService/interface/MetaData.h"
#include "CondCore/MetaDataService/interface/MetaDataNames.h"
#include "CondCore/DBCommon/interface/ServiceLoader.h"
#include "CondCore/DBCommon/interface/Exception.h"
#include "RelationalAccess/IRelationalService.h"
#include "RelationalAccess/RelationalServiceException.h"
#include "RelationalAccess/IRelationalDomain.h"
#include "RelationalAccess/SchemaException.h"
#include "RelationalAccess/ISession.h"
#include "RelationalAccess/ITransaction.h"
#include "RelationalAccess/ISchema.h"
#include "RelationalAccess/ITable.h"
#include "RelationalAccess/TableDescription.h"
#include "RelationalAccess/ITablePrivilegeManager.h"
#include "RelationalAccess/IPrimaryKey.h"
#include "RelationalAccess/ICursor.h"
#include "RelationalAccess/IQuery.h"
#include "RelationalAccess/ITableDataEditor.h"
#include "CoralBase/Exception.h"
#include "CoralBase/AttributeList.h"
#include "CoralBase/AttributeSpecification.h"
#include "CoralBase/Attribute.h"
#include "SealKernel/Exception.h"
cond::MetaData::MetaData(const std::string& connectionString, cond::ServiceLoader& loader):
  m_con(connectionString),m_table(0),m_loader(loader){
  if(!m_loader.hasMessageService()){
    m_loader.loadMessageService();
  }
  if( !m_loader.hasAuthenticationService() ){
    m_loader.loadAuthenticationService();
  }
  coral::IRelationalService& relationalService=m_loader.loadRelationalService();
  coral::IRelationalDomain& domain = relationalService.domainForConnection(m_con);
  m_session.reset( domain.newSession( m_con ) );
  try{
    m_session->connect();
    m_session->startUserSession();
  }catch(seal::Exception& er){
    throw cond::Exception(std::string("MetaData::MetaData caught seal exception ")+er.what());
  }catch(...){
    throw cond::Exception( "MetaData::MetaData caught unknown exception" );
  }
}

cond::MetaData::~MetaData(){
  m_session->disconnect();
}

bool cond::MetaData::addMapping(const std::string& name, const std::string& iovtoken){
  try{
    m_session->transaction().start();
  }catch(seal::Exception& er){
    throw cond::Exception( std::string("MetaData::addMapping Could not start transaction")+ er.what());
  }catch(...){
    throw cond::Exception( "MetaData::addMapping caught unknown exception" );
  }
  try{
    if(!m_session->nominalSchema().existsTable(cond::MetaDataNames::metadataTable())){
      this->createTable(cond::MetaDataNames::metadataTable());
    }else{
      m_table=&(m_session->nominalSchema().tableHandle(cond::MetaDataNames::metadataTable()));
    }
    coral::AttributeList rowBuffer;
    coral::ITableDataEditor& dataEditor = m_table->dataEditor();
    dataEditor.rowBuffer( rowBuffer );
    rowBuffer[cond::MetaDataNames::tagColumn()].data<std::string>()=name;
    rowBuffer[cond::MetaDataNames::tokenColumn()].data<std::string>()=iovtoken;
    dataEditor.insertRow( rowBuffer );
    m_session->transaction().commit() ;
  }catch(seal::Exception& er){
    throw cond::Exception(std::string("MetaData::addMapping Could not commit the transaction ")+er.what() );
  }catch(...){
    throw cond::Exception( "MetaData::addMapping Could not commit the transaction" );
  }
  return true;
}

const std::string cond::MetaData::getToken( const std::string& name ){
  try{
    m_session->transaction().start();
  }catch(const seal::Exception& er){
    throw cond::Exception( std::string("MetaData::getToken: Could not start a new transaction" )+er.what() );
  }catch(...){
    throw cond::Exception( "MetaData::getToken Could not start a new transaction" );
  }
  if(!m_table){
    m_table=&(m_session->nominalSchema().tableHandle( cond::MetaDataNames::metadataTable() ));
  }
  std::string iovtoken;
  std::auto_ptr< coral::IQuery > query( m_session->nominalSchema().newQuery() );
  query->setRowCacheSize( 100 );
  coral::AttributeList emptyBindVariableList;
  std::string condition=cond::MetaDataNames::tagColumn()+"='"+name+"'";
  query->setCondition( condition, emptyBindVariableList );
  query->addToOutputList( cond::MetaDataNames::tokenColumn() );
  coral::ICursor& cursor = query->execute();
  while( cursor.next() ) {
    const coral::AttributeList& row = cursor.currentRow();
    iovtoken=row[ cond::MetaDataNames::tokenColumn() ].data<std::string>();
  }
  try{
    m_session->transaction().commit();
  }catch(const seal::Exception& er){
    throw cond::Exception( std::string("MetaData::getToken Could not commit a transaction")+er.what() );
  }catch(...){
    throw cond::Exception( "MetaData::getToken: Could not commit a transaction" );
  }
  return iovtoken;
}
void cond::MetaData::createTable(const std::string& tabname){
  coral::ISchema& schema=m_session->nominalSchema();
  coral::TableDescription description;
  description.setName( tabname );
  description.insertColumn(  cond::MetaDataNames::tagColumn(), coral::AttributeSpecification::typeNameForId( typeid(std::string)) );
  description.insertColumn( cond::MetaDataNames::tokenColumn(), coral::AttributeSpecification::typeNameForId( typeid(std::string)) );
  std::vector<std::string> cols;
  cols.push_back( cond::MetaDataNames::tagColumn() );
  description.setPrimaryKey(cols);
  description.setNotNullConstraint( cond::MetaDataNames::tokenColumn() );
  coral::ITable& table=schema.createTable(description);
  table.privilegeManager().grantToPublic( coral::ITablePrivilegeManager::Select);
  m_table=&table;
}
