/*
 * Auther: 
 * Date: 2022-09-15
 * Email: vlicecream@163.com
 * */

#include "Exception.h"
#include "Utility.h"

Exception::Exception(const std::string& description) : m_description(description), m_stack(Utility::Stack()) 
{
}

 const std::string Exception::ToString() const
 {
   return m_description;
 }

const std::string Exception::Stack() const
 {
   return m_stack;
 }
